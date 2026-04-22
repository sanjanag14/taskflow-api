# TaskFlow API

A lightweight C++ REST API for task management built using cpp-httplib and nlohmann/json.

## Features
- Full CRUD operations (Create, Read, Update, Delete)
- Task state management (todo, in_progress, done)
- JSON-based persistence
- Thread-safe request handling using mutexes
- Analytics endpoint for task statistics

## Endpoints

- GET /tasks
- POST /tasks
- PUT /tasks/:id
- DELETE /tasks/:id
- GET /stats

## Tech Stack

- C++17
- cpp-httplib
- nlohmann/json
- CMake

## How to Run

1. Open project in CLion
2. Build and run
3. Server runs at:
   http://localhost:8080

## Example Request

```bash
curl -X POST http://localhost:8080/tasks \
-H "Content-Type: application/json" \
-d '{"title":"Example","description":"Test","status":"todo","priority":5}'
