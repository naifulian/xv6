import { ROUTES } from "./utils.js";

const DEFAULT_ROUTE = "processes";

function normalizeRoute(hash) {
  const token = `${hash || `#/${DEFAULT_ROUTE}`}`.replace(/^#\/?/, "").trim();
  if (ROUTES.includes(token)) return token;
  return DEFAULT_ROUTE;
}

export function createRouter(onChange) {
  const handleChange = () => onChange(normalizeRoute(window.location.hash));

  return {
    start() {
      window.addEventListener("hashchange", handleChange);
      if (!window.location.hash) {
        window.location.hash = `#/${DEFAULT_ROUTE}`;
        return;
      }
      handleChange();
    },
    go(route) {
      window.location.hash = `#/${route}`;
    },
  };
}
