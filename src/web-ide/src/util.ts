import { Text } from "@codemirror/state";

export function notify(message: string) {
    const notifMsg = document.getElementById("notif-msg");
    notifMsg.innerText = message;
    notifMsg.style.top = "3rem";
    setTimeout(() => {
      notifMsg.style.top = "-10rem";
    }, 1500);
}

export function createQueryLink(query: Text) {
    const q = encodeURIComponent(String(query));
    const currentUrl = new URL(window.location.href);

    return `${ currentUrl.origin }?query=${ q }`;
}

export function getColorScheme() {
  return window
    .getComputedStyle(document.documentElement)
    .getPropertyValue('content')
    .replace(/"/g, '');
}