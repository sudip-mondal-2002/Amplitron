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
        .catch(function (e) {
          console.error(e);
        })
    );
  });
} else {
  // Window context — register the service worker
  (async function () {
    if (window.crossOriginIsolated !== false) return;

    const registration = await navigator.serviceWorker
      .register(window.document.currentScript.src)
      .catch((e) =>
        console.error("COOP/COEP Service Worker failed to register:", e)
      );
    if (registration) {
      console.log("COOP/COEP Service Worker registered. Reloading page...");
      // Wait for the SW to be active, then reload
      if (registration.installing) {
        const sw = registration.installing || registration.waiting;
        await new Promise((resolve) => {
          sw.addEventListener("statechange", (e) => {
            if (e.target.state === "activated") {
              resolve();
            }
          });
        });
      }
      window.location.reload();
    }
  })();
}
