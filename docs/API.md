# Формат API и JSON-инструкций
WEB-AGENT взаимодействует с сервером по протоколу HTTP/HTTPS путем отправки POST-запросов с телом в формате JSON.

## 1. API Сервера

### 1.1 `POST /api/v1/agent/register`
**Описание:** Регистрация агента и получение сессии.
**Запрос:**
```json
{
  "uid": "agent-001"
}
```
**Ответ (200 OK):**
```json
{
  "status": "ok",
  "session_id": "sess-a1b2c3d4"
}
```
**Ответ (403 Forbidden):** Агент заблокирован на сервере (запрет регистрации).

### 1.2 `POST /api/v1/agent/task`
**Описание:** Запрос новой задачи на выполнение.
**Запрос:**
```json
{
  "uid": "agent-001",
  "session_id": "sess-a1b2c3d4"
}
```
**Ответ (200 OK) - Есть задача:**
```json
{
  "status": "ok",
  "task": {
    "id": "task-999",
    "type": "<тип_задачи>",
    "instruction": { /* параметры */ }
  }
}
```
**Ответ (200 OK) - Нет задачи:**
```json
{
  "status": "no_task"
}
```
*(Вместо этого может использоваться код 204 No Content)*

### 1.3 `POST /api/v1/agent/result`
**Описание:** Отправка результата выполнения задачи на сервер. Для передачи больших файлов должен использоваться `multipart/form-data`, но текстовый лог передаётся в JSON.
**Запрос:**
```json
{
  "uid": "agent-001",
  "session_id": "sess-a1b2c3d4",
  "task_id": "task-999",
  "exit_code": 0,
  "execution_log": "Текст стандартного вывода (stdout + stderr)"
}
```
**Ответ (200 OK):**
```json
{
  "status": "ok"
}
```

## 2. Формат JSON-инструкций (внутри объекта `task`)

### 2.1 Выполнение системной команды (`system_command`)
```json
{
  "type": "system_command",
  "id": "task-001",
  "instruction": {
    "command": "uname -a"
  }
}
```

### 2.2 Запуск программы (`run_program`)
```json
{
  "type": "run_program",
  "id": "task-002",
  "instruction": {
    "executable": "/usr/bin/python3",
    "args": ["script.py", "--verbose"],
    "working_dir": "/tmp"
  }
}
```

### 2.3 Передача файла на сервер (`upload_file`)
```json
{
  "type": "upload_file",
  "id": "task-003",
  "instruction": {
    "file_path": "/var/log/syslog"
  }
}
```
*Примечание:* Сервер в ответ на результат этой команды будет ждать multipart HTTP запрос с файлом на специальный endpoint, например `/api/v1/agent/upload`, в котором будет указан `task_id` и сам файл.
