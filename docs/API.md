# Формат API и JSON-инструкций
WEB-AGENT взаимодействует с сервером по протоколу HTTP/HTTPS.

## 1. API Сервера

### 1.1 Регистрация Веб-агента
**URI:** `https://xdev.arkcom.ru:9999/app/webagent1/api/wa_reg/`
**Method:** POST
**Запрос (JSON):**
```json
{
  "UID": "007",
  "descr": "web-agent"
}
```
**Ответ (JSON) - Успех:**
```json
{
  "code_responce": "0",
  "msg": "Регистрация прошла успешно",
  "access_code": "594807-1ddb-36af-9616-d8ed2b9d"
}
```
**Ответ (JSON) - Агент уже зарегистрирован (или ошибка):**
```json
{
  "code_responce": "-3",
  "msg": "Такой агент уже зарегистрирован"
}
```

### 1.2 Запрос на задание
**URI:** `https://xdev.arkcom.ru:9999/app/webagent1/api/wa_task/`
**Method:** POST
**Запрос (JSON):**
```json
{
  "UID": "007",
  "descr": "web-agent",
  "access_code": "12588b-3d8c-718e-55f4-6ed26b57"
}
```
**Ответ (JSON) - Задание есть (1):**
```json
{
  "code_responce": "1",
  "task_code": "CONF",
  "options": {},
  "session_id": "bvLeD2gv-gtKH-IhmW-rsfd-Ejn1kyweawwi",
  "status": "RUN"
}
```
**Ответ (JSON) - Задания нет (0):**
```json
{
  "code_responce": "0",
  "status": "WAIT"
}
```
**Ответ (JSON) - Ошибка в запросе (< 0):**
```json
{
  "code_responce": "-2",
  "msg": "неверный код доступа"
}
```

### 1.3 Результат выполнения задания
**URI:** `https://xdev.arkcom.ru:9999/app/webagent1/api/wa_result/`
**Method:** POST
**Запрос (multipart/form-data):**
Поля:
* `result_code`: код результата (0 - ок; < 0 - код ошибки)
* `result`: JSON строка с содержанием действий по заданию
* `file1`: файл 1 (если есть)
* `file2`: файл 2 (если есть)
* `file3`: файл 3 ...

**Структура JSON в поле `result`:**
```json
{
  "UID": "007",
  "access_code": "12588b-3d8c-718e-55f4-6ed26b57",
  "message": "задание выполнено" (или "задание не выполнено"),
  "files": 3,
  "session_id": "ieLOLGzL-nyGP-mfG5-m3nI-eYL1CZzcaXzO"
}
```

**Ответ (JSON) - Успех:**
```json
{
  "code_responce": "0",
  "msg": "ok"
}
```
**Ответ (JSON) - Ошибка загрузки файлов:**
```json
{
  "code_responce": "-3",
  "msg": "не все файлы загружены",
  "status": "ERROR"
}
```

## 2. Логика полей `task_code` и `options` (внутри ответа на задание)
При получении задачи сервер возвращает `task_code` (например, "CONF", "SYS_CMD", "RUN_PROG") и `options` (параметры выполнения, обычно JSON-строка или текст аргументов).
Агент парсит значение и запускает соответствующий обработчик.
