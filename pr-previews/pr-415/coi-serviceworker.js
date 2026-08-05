/*! coi-serviceworker v0.1.7 - Guido Zuidhof, licensed under MIT */
/*
 * This service worker intercepts all fetch requests and adds the
 * Cross-Origin-Embedder-Policy and Cross-Origin-Opener-Policy headers
 * required for SharedArrayBuffer to work. This is needed for Emscripten
 * pthreads / AudioWorklet on hosts like GitHub Pages that don't allow
 * setting custom HTTP headers.
 *
 * Source: https://github.com/nicbarker/clay/blob/main/examples/shared/coi-serviceworker.js
 */
if (typeof window === 'undefined') {
  // Service Worker context
  self.addEventListener("install", () => self.skipWaiting());
  self.addEventListener("activate", (e) => e.waitUntil(self.clients.claim()));

  self.addEventListener("message", (ev) => {
    if (ev.data && ev.data.type === "deregister") {
      self.registration
        .unregister()
        .then(() => {
          return self.clients.matchAll();
        })
        .then((clients) => {
          clients.forEach((client) => client.navigate(client.url));
        });
    }
  });

  self.addEventListener("fetch", function (event) {
    if (
      event.request.cache === "only-if-cached" &&
      event.request.mode !== "same-origin"
    ) {
      return;
    }

    event.respondWith(
      fetch(event.request)
        .then(function (response) {
          if (response.status === 0) {
            return response;
          }

          const newHeaders = new Headers(response.headers);
          newHeaders.set("Cross-Origin-Embedder-Policy", "require-corp");
          newHeaders.set("Cross-Origin-Opener-Policy", "same-origin");

          return new Response(response.body, {
            status: response.status,
            statusText: response.statusText,
            headers: newHeaders,
          });
        })
        .catch((e) => {
          console.error("Fetch failed:", e);
          return new Response("Network error",
            { status: 503,
              statusText: "Service Unavailable",
              headers: { "Content-Type": "text/plain", },
            });
        })
    );
  });
} else {
  // Window context — register the service worker
  (async function () {
    if (!("serviceWorker" in navigator)){
      console.warn("Service workers are not supported in this browser.");
      return;
    }
    if (window.crossOriginIsolated !== false) {
      sessionStorage.removeItem("coiReloaded");
      return;
    }

    const scriptUrl = window.document.currentScript?.src;

if (!scriptUrl) {
  console.warn("Unable to determine current service worker script URL.");
  return;
}

const existingRegistration = await navigator.serviceWorker.getRegistration();

const existingScriptUrl =
  existingRegistration?.active?.scriptURL ??
  existingRegistration?.waiting?.scriptURL ??
  existingRegistration?.installing?.scriptURL;

if (existingScriptUrl === scriptUrl) {
  console.log("COOP/COEP Service Worker already registered.");
  return;
}

const registration = await navigator.serviceWorker
      .register(scriptUrl)
      .catch((e) =>
        console.error("COOP/COEP Service Worker failed to register:", e)
      );
    if (registration) {
      console.log("COOP/COEP Service Worker registered. Reloading page...");
      // Wait for the SW to be active, then reload
      if (registration.installing) {
        const sw = registration.installing || registration.waiting;
        await new Promise((resolve) => {
          const sw = registration.installing || registration.waiting;
          if (!sw) return resolve();
          const onStateChange = () => {
            if (sw.state === "activated") {
              sw.removeEventListener("statechange", onStateChange);
              resolve();
            }
          };
          sw.addEventListener("statechange", onStateChange);
        });
      }
      const RELOAD_KEY = "coiReloaded";
      if (!sessionStorage.getItem(RELOAD_KEY)) {
        sessionStorage.setItem(RELOAD_KEY, "true");
        window.location.reload();
      }
    }
  })();
}
