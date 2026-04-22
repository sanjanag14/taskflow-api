#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>

#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

struct Task {
    int id;
    std::string title;
    std::string description;
    std::string status;   // todo, in_progress, done
    int priority;         // 1-5
};

std::vector<Task> tasks;
std::mutex tasksMutex;
const std::string DATA_FILE = "../data/tasks.json";

void to_json(json& j, const Task& t) {
    j = json{
        {"id", t.id},
        {"title", t.title},
        {"description", t.description},
        {"status", t.status},
        {"priority", t.priority}
    };
}

void from_json(const json& j, Task& t) {
    j.at("id").get_to(t.id);
    j.at("title").get_to(t.title);
    j.at("description").get_to(t.description);
    j.at("status").get_to(t.status);
    j.at("priority").get_to(t.priority);
}

bool isValidStatus(const std::string& status) {
    return status == "todo" || status == "in_progress" || status == "done";
}

void saveTasks() {
    std::lock_guard<std::mutex> lock(tasksMutex);
    json j = tasks;

    std::ofstream out(DATA_FILE);
    if (!out) {
        std::cerr << "Failed to open data file for writing.\n";
        return;
    }

    out << j.dump(4);
}

void loadTasks() {
    std::lock_guard<std::mutex> lock(tasksMutex);
    std::ifstream in(DATA_FILE);

    if (!in) {
        std::cerr << "No existing task file found. Starting fresh.\n";
        tasks.clear();
        return;
    }

    if (in.peek() == std::ifstream::traits_type::eof()) {
        tasks.clear();
        return;
    }

    try {
        json j;
        in >> j;
        tasks = j.get<std::vector<Task>>();
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse tasks.json: " << e.what() << "\n";
        tasks.clear();
    }
}

int getNextId() {
    int maxId = 0;
    for (const auto& task : tasks) {
        if (task.id > maxId) {
            maxId = task.id;
        }
    }
    return maxId + 1;
}

int main() {
    loadTasks();

    httplib::Server server;

    server.Get("/", [](const httplib::Request&, httplib::Response& res) {
        json response = {
            {"message", "TaskFlow API is running"},
            {"endpoints", {
                "GET /tasks",
                "POST /tasks",
                "PUT /tasks/<id>",
                "DELETE /tasks/<id>",
                "GET /stats"
            }}
        };
        res.set_content(response.dump(4), "application/json");
    });

    server.Get("/tasks", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(tasksMutex);
        json j = tasks;
        res.set_content(j.dump(4), "application/json");
    });

    server.Get("/stats", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(tasksMutex);

        int todo = 0;
        int inProgress = 0;
        int done = 0;

        for (const auto& task : tasks) {
            if (task.status == "todo") {
                todo++;
            } else if (task.status == "in_progress") {
                inProgress++;
            } else if (task.status == "done") {
                done++;
            }
        }

        json stats = {
            {"total", tasks.size()},
            {"todo", todo},
            {"in_progress", inProgress},
            {"done", done}
        };

        res.set_content(stats.dump(4), "application/json");
    });

    server.Post("/tasks", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);

            if (!body.contains("title") || !body.contains("description") ||
                !body.contains("status") || !body.contains("priority")) {
                res.status = 400;
                res.set_content(R"({"error":"Missing required fields"})", "application/json");
                return;
            }

            std::string status = body["status"];
            int priority = body["priority"];

            if (!isValidStatus(status)) {
                res.status = 400;
                res.set_content(R"({"error":"Status must be todo, in_progress, or done"})", "application/json");
                return;
            }

            if (priority < 1 || priority > 5) {
                res.status = 400;
                res.set_content(R"({"error":"Priority must be between 1 and 5"})", "application/json");
                return;
            }

            Task newTask;

            {
                std::lock_guard<std::mutex> lock(tasksMutex);
                newTask.id = getNextId();
                newTask.title = body["title"];
                newTask.description = body["description"];
                newTask.status = status;
                newTask.priority = priority;
                tasks.push_back(newTask);
            }

            saveTasks();

            json response = newTask;
            res.status = 201;
            res.set_content(response.dump(4), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json error = {{"error", std::string("Invalid JSON: ") + e.what()}};
            res.set_content(error.dump(4), "application/json");
        }
    });

    server.Put(R"(/tasks/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            json body = json::parse(req.body);

            Task updatedTask;
            bool found = false;

            {
                std::lock_guard<std::mutex> lock(tasksMutex);
                auto it = std::find_if(tasks.begin(), tasks.end(),
                                       [id](const Task& t) { return t.id == id; });

                if (it == tasks.end()) {
                    res.status = 404;
                    res.set_content(R"({"error":"Task not found"})", "application/json");
                    return;
                }

                if (body.contains("title")) {
                    it->title = body["title"];
                }

                if (body.contains("description")) {
                    it->description = body["description"];
                }

                if (body.contains("status")) {
                    std::string status = body["status"];
                    if (!isValidStatus(status)) {
                        res.status = 400;
                        res.set_content(R"({"error":"Status must be todo, in_progress, or done"})", "application/json");
                        return;
                    }
                    it->status = status;
                }

                if (body.contains("priority")) {
                    int priority = body["priority"];
                    if (priority < 1 || priority > 5) {
                        res.status = 400;
                        res.set_content(R"({"error":"Priority must be between 1 and 5"})", "application/json");
                        return;
                    }
                    it->priority = priority;
                }

                updatedTask = *it;
                found = true;
            }

            if (found) {
                saveTasks();
                json response = updatedTask;
                res.set_content(response.dump(4), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            json error = {{"error", std::string("Invalid request: ") + e.what()}};
            res.set_content(error.dump(4), "application/json");
        }
    });

    server.Delete(R"(/tasks/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int id = std::stoi(req.matches[1]);
            bool deleted = false;

            {
                std::lock_guard<std::mutex> lock(tasksMutex);
                auto oldSize = tasks.size();

                tasks.erase(
                    std::remove_if(tasks.begin(), tasks.end(),
                                   [id](const Task& t) { return t.id == id; }),
                    tasks.end()
                );

                deleted = (tasks.size() != oldSize);
            }

            if (!deleted) {
                res.status = 404;
                res.set_content(R"({"error":"Task not found"})", "application/json");
                return;
            }

            saveTasks();
            res.set_content(R"({"message":"Task deleted successfully"})", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json error = {{"error", std::string("Invalid request: ") + e.what()}};
            res.set_content(error.dump(4), "application/json");
        }
    });

    std::cout << "Server running at http://localhost:8080\n";
    server.listen("0.0.0.0", 8080);

    return 0;
}