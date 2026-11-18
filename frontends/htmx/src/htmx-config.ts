// htmx runtime configuration for the htmx frontend.
declare const htmx: { config: Record<string, unknown> };
htmx.config.historyCacheSize = 0; // always refetch on back/forward
htmx.config.scrollIntoViewOnBoost = false;
