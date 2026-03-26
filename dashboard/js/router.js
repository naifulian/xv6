import { ROUTES } from "./utils.js";

function normalizeRoute(hash) {
  const token = `${hash || "#/home"}`.replace(/^#\/?/, "").trim();
  if (ROUTES.includes(token)) return token;
  return "home";
}

export function createRouter(onChange) {
  const handleChange = () => onChange(normalizeRoute(window.location.hash));

  return {
    start() {
      window.addEventListener("hashchange", handleChange);
      if (!window.location.hash) {
        window.location.hash = "#/home";
        return;
      }
      handleChange();
    },
    go(route) {
      window.location.hash = `#/${route}`;
    },
  };
}
