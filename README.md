# WEB-AGENT

**WEB-AGENT** — это высокопроизводительное, кроссплатформенное клиентское приложение на языке C++, предназначенное для автоматизированного выполнения удаленных команд и сценариев под управлением централизованного сервера.

[![C++](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)](#)
[![CI](https://github.com/post-kserks/webagent/actions/workflows/ci.yml/badge.svg)](https://github.com/post-kserks/webagent/actions/workflows/ci.yml)
[![CD Release](https://github.com/post-kserks/webagent/actions/workflows/cd-release.yml/badge.svg)](https://github.com/post-kserks/webagent/actions/workflows/cd-release.yml)
[![Release](https://img.shields.io/github/v/release/post-kserks/webagent)](https://github.com/post-kserks/webagent/releases)

---

## Обзор

Проект представляет собой автономного агента, работающего в фоновом режиме. Он обеспечивает надежный канал взаимодействия между локальной машиной и управляющим сервером по протоколу HTTP/HTTPS. Агент предназначен для динамического получения задач, их выполнения и оперативной передачи результатов (включая файлы логов и выходные данные) обратно на сервер.

### Основные сценарии использования
*   **Удаленный мониторинг:** Сбор системных логов и метрик.
*   **Автоматизация задач:** Дистанционный запуск скриптов и программ.
*   **Управление контентом:** Загрузка и воспроизведение медиафайлов по требованию.

---

## Ключевые особенности

*   **Кроссплатформенность:** Полная поддержка Windows, Linux и macOS (используются нативные механизмы исполнения команд).
*   **Автономность:** Интеллектуальный механизм регистрации (Session Management) с сохранением состояния между перезапусками.
*   **Надежность:** Реализация стратегии *Exponential Backoff* для обработки временной недоступности сети или сервера.
*   **Гибкая конфигурация:** Управление через структурированные JSON-файлы.
*   **Минимализм:** Низкое потребление системных ресурсов (ЦП и ОЗУ) благодаря эффективной реализации на C++.

---

## Архитектура

Система построена на модульной архитектуре, обеспечивающей простоту расширения и поддержки:

```mermaid
flowchart LR
    AGENT["Agent"]
    SERVER["Server"]

    AGENT -- "interacts with" --> SERVER

    subgraph SERVER_ZONE["Server Actions"]
        direction LR
        SA1([Process Task Execution Request])
        SA2([Register Agent])
        SA3([Agree Instruction Format])
        SA4([Receive Results from Agent])

        SB1([Issue Task Instructions])
        SB2([No Task Available])
        SB3([Handle Registration Refusal])
        SB4([Acknowledge / Error Code])
        SB5([Issue Session Number])
        SB6([Issue User ID])

        SA1 -. "if task available" .-> SB1
        SA1 -. "if no task" .-> SB2
        SA1 -. "on failure" .-> SB3
        SA2 -. "process results" .-> SB4
        SA3 -. "on success" .-> SB5
        SA4 -. "on success" .-> SB6
    end

    subgraph AGENT_ZONE["Agent Actions"]
        direction LR
        AA1([Execute Task])
        AA2([Log Actions and Results])
        AA3([Check Server Availability])
        AA4([Register on Server])
        AA5([Request Task Execution])

        AB1([Transfer Files])
        AB2([Send Data to Server])
        AB3([Increase Request Interval])
        AB4([Send UID and Get Confirmation])
        AB5([Handle Registration Failure])
        AB6([Receive Instruction])

        AC1([Send Execution Code/Error])
        AC2([Send Session Number])
        AC3([Report Errors])
        AC4([Wait for Instruction])

        AA1 --> AB1
        AA1 --> AB2
        AA2 --> AB2
        AA3 -- "if failed" --> AB3
        AA4 --> AB4
        AA4 -. "if failed" .-> AB5
        AA5 -. "if available" .-> AB6
        AB6 -. "if no instruction" .-> AC4
        AB2 --> AC1
        AB2 --> AC2
        AB2 --> AC3
    end

    subgraph SETTINGS["Agent Settings"]
        direction LR
        ST1([Server Address])
        ST2([Session])
        ST3([Agent UID])
        ST4([Action Log])
        ST5([Result Folder])
        ST6([Task Folder])
        ST7(["Program Settings (PATH ENV)"])
    end

    SERVER --> SA1
    SERVER --> SA2
    SERVER --> SA3
    SERVER --> SA4

    AGENT --> AA1
    AGENT --> AA2
    AGENT --> AA3
    AGENT --> AA4
    AGENT --> AA5
    AGENT --> AB2
    AGENT --> AB6

    classDef actor fill:#ffffff,stroke:#444,color:#111,stroke-width:1px;
    classDef usecase fill:#f4f4f4,stroke:#888,color:#111,stroke-width:1px;
    class AGENT,SERVER actor;
    class SA1,SA2,SA3,SA4,SB1,SB2,SB3,SB4,SB5,SB6,AA1,AA2,AA3,AA4,AA5,AB1,AB2,AB3,AB4,AB5,AB6,AC1,AC2,AC3,AC4,ST1,ST2,ST3,ST4,ST5,ST6,ST7 usecase;
```

*   **Agent Core:** Центральный цикл управления (регистрация -> опрос -> выполнение).
*   **SessionManager:** Управление UID и безопасными кодами доступа (Access Code).
*   **TaskExecutor:** Изолированный запуск системных процессов и управление ресурсами.
*   **HttpClient:** Нативная обертка над сетевыми утилитами (curl/wget) для взаимодействия с API.

---

## Быстрый старт

### Требования
*   Компилятор C++ с поддержкой стандарта **C++17** и выше.
*   **CMake** версии 3.10 или старше.
*   Установленные системные утилиты: `curl` (Windows) или `wget`/`curl` (Linux/macOS).

### Сборка
1. Клонируйте репозиторий:
   ```bash
   git clone https://github.com/your-repo/webagent.git
   cd webagent
   ```
2. Создайте директорию для сборки и скомпилируйте проект:
   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build .
   ```

---

## Конфигурация

Настройка агента осуществляется через файл `config/config.json`. 

| Параметр | Описание | Пример |
| :--- | :--- | :--- |
| `uid` | Уникальный идентификатор агента | `"agent_007"` |
| `descr` | Описание агента для сервера | `"web-agent"` |
| `server_uri` | Базовый URL управляющего сервера | `"https://api.server.com/"` |
| `request_interval` | Частота опроса сервера (в секундах) | `5` |
| `max_retry_interval` | Максимальный интервал при потере соединения (Exponential Backoff) | `60` |
| `local_control_port` | Локальный API для связи фронта и агента | `8787` |
| `log_file` | Путь к файлу журнала событий | `"./logs/agent.log"` |
| `tasks_folder` | Директория для хранения скачанных файлов задач | `"./tasks"` |
| `results_folder` | Директория с файлами-результатами (отправляются при `FILE`) | `"./results"` |
| `path_env` | Переопределение переменной PATH (для curl/wget в фоновом режиме) | `"/usr/local/bin:/usr/bin:/bin"` |

Пример файла:
```json
{
  "uid": "bmstu_agent_01",
  "descr": "web-agent",
  "server_uri": "https://xdev.arkcom.ru:9999/app/api/",
  "request_interval": 10,
  "max_retry_interval": 60,
  "local_control_port": 8787,
  "log_file": "./logs/agent.log",
  "tasks_folder": "./tasks",
  "results_folder": "./results",
  "path_env": "/usr/local/bin:/usr/bin:/bin"
}
```

---

## Использование

После успешной сборки запустите исполняемый файл:

```bash
./webagent
```

По умолчанию агент при старте автоматически открывает фронтенд `frontend/index.html` в браузере.
Если нужно запустить только агент без UI:

```bash
./webagent --no-frontend
```

Фронтенд синхронизирует выбранный видеофайл с агентом через локальный endpoint:

`POST http://127.0.0.1:<local_control_port>/api/selected-video`

### Принципы работы
1.  **Регистрация:** При первом запуске агент регистрируется на сервере и получает `access_code`, который сохраняется локально в скрытом файле.
2.  **Опрос задач:** Агент периодически запрашивает задачи у сервера.
3.  **Исполнение:** 
    *   `TASK`: Выполнение специфических сценариев (например, загрузка и открытие видео).
    *   `FILE`: Сбор и отправка логов выполнения на сервер.
    *   `CONF`: Дистанционное завершение работы или обновление конфигурации.

---
