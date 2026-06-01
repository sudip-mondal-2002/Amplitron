/* Consolidated scripts for docs/index.html */

/* === PostHog (moved from inline <script>) === */
!(function (t, e) {
    var o, n, p, r
    e.__SV ||
        (window.posthog && window.posthog.__loaded) ||
        ((window.posthog = e),
        (e._i = []),
        (e.init = function (i, s, a) {
            function g(t, e) {
                var o = e.split(".")
                ;(2 == o.length && ((t = t[o[0]]), (e = o[1])),
                    (t[e] = function () {
                        t.push(
                            [e].concat(Array.prototype.slice.call(arguments, 0))
                        )
                    }))
            }
            ;(((p = t.createElement("script")).type = "text/javascript"),
                (p.crossOrigin = "anonymous"),
                (p.async = !0),
                (p.src =
                    s.api_host.replace(
                        ".i.posthog.com",
                        "-assets.i.posthog.com"
                    ) + "/static/array.js"),
                (r = t.getElementsByTagName("script")[0]).parentNode.insertBefore(
                    p,
                    r
                ))
            var u = e
            for (
                void 0 !== a ? (u = e[a] = []) : (a = "posthog"),
                    u.people = u.people || [],
                    u.toString = function (t) {
                        var e = "posthog"
                        return (
                            "posthog" !== a && (e += "." + a),
                            t || (e += " (stub)"),
                            e
                        )
                    },
                    u.people.toString = function () {
                        return u.toString(1) + ".people (stub)"
                    },
                    o =
                        "ci init Pi Ci ft Oi Fi ki capture calculateEventProperties Ui register register_once register_for_session unregister unregister_for_session Bi getFeatureFlag getFeatureFlagPayload getFeatureFlagResult isFeatureEnabled reloadFeatureFlags updateFlags updateEarlyAccessFeatureEnrollment getEarlyAccessFeatures on onFeatureFlags onSurveysLoaded onSessionId getSurveys getActiveMatchingSurveys renderSurvey displaySurvey cancelPendingSurvey canRenderSurvey canRenderSurveyAsync identify setPersonProperties group resetGroups setPersonPropertiesForFlags resetPersonPropertiesForFlags setGroupPropertiesForFlags resetGroupPropertiesForFlags reset get_distinct_id getGroups get_session_id get_session_replay_url alias set_config startSessionRecording stopSessionRecording sessionRecordingStarted captureException startExceptionAutocapture stopExceptionAutocapture loadToolbar get_property getSessionProperty ji Di createPersonProfile setInternalOrTestUser zi Ti Hi opt_in_capturing opt_out_capturing has_opted_in_capturing has_opted_out_capturing get_explicit_consent_status is_capturing clear_opt_in_out_capturing Ai debug bt Ni getPageViewId captureTraceFeedback captureTraceMetric Ei".split(
                            " "
                        ),
                    n = 0;
                n < o.length;
                n++
            )
                g(u, o[n])
            e._i.push([i, s, a])
        }),
        (e.__SV = 1))
})(document, window.posthog || [])

posthog.init("phc_ecTrgEi1Wfx1rgTVwSZxCWt9swjuWiKaXAYssHhHLNr", {
    api_host: "https://t.amplitron.sudipmondal.co.in",
    ui_host: "https://us.posthog.com",
    person_profiles: "identified_only",
})

/* === Page interactions (moved from inline <script>) === */
document.addEventListener("DOMContentLoaded", () => {
    const navbar = document.getElementById("navbar")
    if (navbar) {
        const onScroll = () => {
            if (window.scrollY > 60) {
                navbar.classList.add("scrolled")
            } else {
                navbar.classList.remove("scrolled")
            }
        }

        window.addEventListener("scroll", onScroll, { passive: true })
        onScroll()
    }

    const hamburger = document.getElementById("hamburger")
    const navLinks = document.getElementById("navLinks")

    if (hamburger && navLinks) {
        hamburger.addEventListener("click", () => {
            navLinks.classList.toggle("active")
            hamburger.textContent = navLinks.classList.contains("active")
                ? "✕"
                : "☰"
        })
    }
})
