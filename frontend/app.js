const state = {
  useMock: true,
  agent: {
    uid: "007_james_bong",
    descr: "web-agent",
    api: "https://xdev.arkcom.ru:9999/app/webagent1/api/",
    interval: 5,
  },
  queue: [],
  nextQueueId: 1,
  controlApiBase: "http://127.0.0.1:8787",
  videoRepo: {
    owner: "testerVsego",
    repo: "vid_for_agent",
    branch: "main",
    path: "",
  },
  videos: [],
  videosLoading: false,
  videosError: "",
  selectedVideo: "",
  activity: [],
};

const elements = {
  mockToggle: document.getElementById("mock-toggle"),
  metricUid: document.getElementById("metric-uid"),
  metricInterval: document.getElementById("metric-interval"),
  metricQueue: document.getElementById("metric-queue"),
  agentForm: document.getElementById("agent-form"),
  taskForm: document.getElementById("task-form"),
  taskCode: document.getElementById("task-code"),
  taskOptions: document.getElementById("task-options"),
  queueBody: document.getElementById("queue-body"),
  activityLog: document.getElementById("activity-log"),
  uidInput: document.getElementById("uid-input"),
  descrInput: document.getElementById("descr-input"),
  apiInput: document.getElementById("api-input"),
  controlApiInput: document.getElementById("control-api-input"),
  intervalInput: document.getElementById("interval-input"),
};

function now() {
  return new Date().toLocaleString("ru-RU");
}

function addActivity(message) {
  state.activity.unshift({ at: now(), message });
  state.activity = state.activity.slice(0, 30);
  renderActivity();
}

function updateMetrics() {
  elements.metricUid.textContent = state.agent.uid;
  elements.metricInterval.textContent = `${state.agent.interval} сек`;
  elements.metricQueue.textContent = String(state.queue.length);
}

async function syncSelectedVideoWithAgent(videoName, silent = false) {
  if (!videoName) {
    return false;
  }

  try {
    const response = await fetch(`${state.controlApiBase}/api/selected-video`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ video: videoName }),
    });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    if (!silent) {
      addActivity(`Видео синхронизировано с агентом: ${videoName}`);
    }
    return true;
  } catch (error) {
    if (!silent) {
      addActivity(`Локальный API агента недоступен: ${error.message}`);
    }
    return false;
  }
}

function videoListStatusText() {
  if (state.videosLoading) {
    return "Загружаю список видео из репозитория...";
  }
  if (state.videosError) {
    return `Не удалось загрузить список видео: ${state.videosError}`;
  }
  if (state.videos.length === 0) {
    return "В репозитории нет mp4-файлов.";
  }
  return `Найдено ${state.videos.length} видео в репозитории.`;
}

function renderTaskOptions() {
  const code = elements.taskCode.value;

  if (code === "TASK") {
    const options = state.videos
      .map((video) => {
        const selected = video.name === state.selectedVideo ? "selected" : "";
        return `<option value="${video.name}" ${selected}>${video.name}</option>`;
      })
      .join("");
    const disabled = state.videos.length === 0 ? "disabled" : "";

    elements.taskOptions.innerHTML = `
      <label>
        Видео из репозитория
        <select id="task-video-select" name="taskOption" ${disabled}>
          ${options || `<option value="">Нет видео</option>`}
        </select>
      </label>
      <div class="inline-form">
        <button id="refresh-videos" type="button">Обновить список видео</button>
        <a class="repo-link" href="https://github.com/${state.videoRepo.owner}/${state.videoRepo.repo}" target="_blank" rel="noreferrer">Открыть репозиторий</a>
      </div>
      <p class="hint">${videoListStatusText()}</p>
    `;
    return;
  }

  if (code === "TIMEOUT") {
    elements.taskOptions.innerHTML = `
      <label>
        Новый интервал опроса (сек)
        <input name="taskOption" type="number" min="1" value="10" />
      </label>
    `;
    return;
  }

  if (code === "FILE") {
    elements.taskOptions.innerHTML = `
      <label>
        Комментарий к запросу
        <input name="taskOption" type="text" placeholder="Например: прислать свежие логи" />
      </label>
      <p class="hint">FILE отправляет текстовый параметр в поле options.</p>
    `;
    return;
  }

  elements.taskOptions.innerHTML = `
    <p class="hint">CONF не требует options, команда завершает цикл агента.</p>
  `;
}

function renderQueue() {
  if (state.queue.length === 0) {
    elements.queueBody.innerHTML = `
      <tr>
        <td colspan="5">Очередь пуста</td>
      </tr>
    `;
    return;
  }

  elements.queueBody.innerHTML = state.queue
    .map((item) => {
      const options = typeof item.options === "string" ? item.options : JSON.stringify(item.options);
      const statusLabels = {
        pending: "pending",
        sent: "sent",
        done: "done",
        error: "error",
      };
      const statusText = statusLabels[item.status] || item.status;
      const action = item.status === "pending"
        ? `<button type="button" data-send-id="${item.id}">Отправить</button>`
        : item.status === "sent"
          ? `<button type="button" disabled>Отправляется...</button>`
          : item.status === "done"
            ? `<button type="button" disabled>Готово</button>`
            : `<button type="button" disabled>Ошибка</button>`;
      return `
        <tr>
          <td>${item.id}</td>
          <td>${item.taskCode}</td>
          <td><code>${escapeHtml(options)}</code></td>
          <td><span class="queue-status ${item.status}">${statusText}</span></td>
          <td>${action}</td>
        </tr>
      `;
    })
    .join("");
}

async function loadVideosFromRepo() {
  state.videosLoading = true;
  state.videosError = "";
  renderTaskOptions();

  const pathSuffix = state.videoRepo.path ? `/${state.videoRepo.path}` : "";
  const apiUrl = `https://api.github.com/repos/${state.videoRepo.owner}/${state.videoRepo.repo}/contents${pathSuffix}?ref=${state.videoRepo.branch}`;

  try {
    const response = await fetch(apiUrl, {
      headers: {
        Accept: "application/vnd.github+json",
      },
    });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    const data = await response.json();
    const items = Array.isArray(data) ? data : [];
    state.videos = items
      .filter((item) => item.type === "file" && item.name.toLowerCase().endsWith(".mp4"))
      .map((item) => ({
        name: item.name,
        downloadUrl: item.download_url,
        htmlUrl: item.html_url,
      }))
      .sort((a, b) => a.name.localeCompare(b.name, "ru", { numeric: true }));

    if (state.videos.length > 0) {
      const stillExists = state.videos.some((video) => video.name === state.selectedVideo);
      if (!stillExists) {
        state.selectedVideo = state.videos[0].name;
      }
      syncSelectedVideoWithAgent(state.selectedVideo, true);
      addActivity(`Список видео обновлён (${state.videos.length} шт.).`);
    } else {
      state.selectedVideo = "";
      addActivity("В репозитории не найдено mp4-файлов.");
    }
  } catch (error) {
    state.videosError = error.message;
    state.videos = [];
    state.selectedVideo = "";
    addActivity(`Ошибка загрузки видео из репозитория: ${error.message}`);
  } finally {
    state.videosLoading = false;
    renderTaskOptions();
  }
}

function renderActivity() {
  if (state.activity.length === 0) {
    elements.activityLog.innerHTML = `<li><span>Пока нет событий.</span></li>`;
    return;
  }

  elements.activityLog.innerHTML = state.activity
    .map((entry) => `<li><time>${entry.at}</time><span>${escapeHtml(entry.message)}</span></li>`)
    .join("");
}

function enqueueTask(taskCode, options) {
  state.queue.unshift({
    id: state.nextQueueId++,
    taskCode,
    options,
    status: "pending",
  });

  updateMetrics();
  renderQueue();
  addActivity(`Задача ${taskCode} добавлена в очередь.`);
}

function buildTaskOptionFromVideoName(videoName) {
  const match = videoName.match(/^screamer(\d+)\.mp4$/i);
  if (match) {
    return match[1];
  }
  return videoName;
}

async function sendTask(item) {
  try {
    item.status = "sent";
    renderQueue();

    const payload = {
      UID: state.agent.uid,
      descr: state.agent.descr,
      task_code: item.taskCode,
      options: item.options,
    };

    if (state.useMock) {
      await fakeNetworkDelay();
    } else {
      const response = await fetch(`${state.agent.api}wa_task/`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }
    }

    item.status = "done";
    addActivity(`Задача #${item.id} обработана (${item.taskCode}).`);
  } catch (error) {
    item.status = "error";
    addActivity(`Ошибка отправки задачи #${item.id}: ${error.message}`);
  }

  renderQueue();
}

function fakeNetworkDelay() {
  return new Promise((resolve) => setTimeout(resolve, 450));
}

function escapeHtml(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function bindEvents() {
  elements.mockToggle.addEventListener("click", () => {
    state.useMock = !state.useMock;
    elements.mockToggle.textContent = state.useMock ? "API: Mock" : "API: Live";
    addActivity(`Режим API переключён: ${state.useMock ? "Mock" : "Live"}.`);
  });

  elements.agentForm.addEventListener("submit", (event) => {
    event.preventDefault();

    state.agent.uid = elements.uidInput.value.trim();
    state.agent.descr = elements.descrInput.value.trim();
    state.agent.api = elements.apiInput.value.trim();
    state.controlApiBase = elements.controlApiInput.value.trim();
    state.agent.interval = Number(elements.intervalInput.value);

    updateMetrics();
    addActivity(`Профиль агента обновлён (${state.agent.uid}).`);
  });

  elements.taskCode.addEventListener("change", renderTaskOptions);

  elements.taskForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    const formData = new FormData(elements.taskForm);
    const taskCode = formData.get("taskCode");
    const rawOption = (formData.get("taskOption") || "").toString().trim();

    if (taskCode === "TASK") {
      const selected = rawOption || state.selectedVideo;
      if (!selected) {
        addActivity("Выберите видео из репозитория перед отправкой TASK.");
        return;
      }
      await syncSelectedVideoWithAgent(selected);
      const taskOption = buildTaskOptionFromVideoName(selected);
      enqueueTask(taskCode, taskOption);
      addActivity(`TASK сформирован: выбран ${selected}, в options отправим "${taskOption}".`);
      return;
    }

    if (taskCode === "CONF") {
      enqueueTask(taskCode, "");
      return;
    }

    if (taskCode === "FILE" && rawOption === "") {
      enqueueTask(taskCode, JSON.stringify({ note: "file-request" }));
      return;
    }

    enqueueTask(taskCode, rawOption);
  });

  elements.taskOptions.addEventListener("change", (event) => {
    const target = event.target;
    if (!(target instanceof HTMLSelectElement)) {
      return;
    }
    if (target.id === "task-video-select") {
      state.selectedVideo = target.value;
      const picked = target.value || "none";
      addActivity(`Выбрано видео: ${picked}`);
      if (target.value) {
        syncSelectedVideoWithAgent(target.value, true);
      }
    }
  });

  elements.taskOptions.addEventListener("click", (event) => {
    const target = event.target;
    if (!(target instanceof HTMLButtonElement)) {
      return;
    }
    if (target.id === "refresh-videos") {
      loadVideosFromRepo();
    }
  });

  elements.queueBody.addEventListener("click", async (event) => {
    const button = event.target;
    if (!(button instanceof HTMLButtonElement)) {
      return;
    }

    const id = Number(button.dataset.sendId);
    const item = state.queue.find((entry) => entry.id === id);
    if (!item) {
      return;
    }

    await sendTask(item);
  });
}

function init() {
  renderTaskOptions();
  renderQueue();
  renderActivity();
  updateMetrics();
  bindEvents();
  addActivity("Интерфейс готов. Можно создавать и отправлять задачи.");
  loadVideosFromRepo();
}

init();
