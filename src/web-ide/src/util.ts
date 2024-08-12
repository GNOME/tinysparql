/**
 * Set notification box text to message given
 * 
 * then animates the box to show it sliding down from the top
 * 
 * Sends the box back after 1.5s
 *
 * @param message - Notification message to show
 */
export function notify(message: string) {
    const notifMsg = document.getElementById("notif-msg");
    notifMsg.innerText = message;
    notifMsg.style.top = "3rem";
    setTimeout(() => {
      notifMsg.style.top = "-10rem";
    }, 1500);
}

/**
 * Returns current color scheme used
 *
 * @returns "dark"/"light"
 */
export function getColorScheme() {
  return window
    .getComputedStyle(document.documentElement)
    .getPropertyValue('content')
    .replace(/"/g, '');
}