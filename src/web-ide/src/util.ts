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
  notifMsg.style.top = "2rem";
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
    .getPropertyValue("content")
    .replace(/"/g, "");
}

/**
 * Generates labelled checkboxes wrapped up nicely
 *
 * Attaches change listener based on arguments given
 *
 * Classes can also be added via the arguments
 *
 * @param label - the label text attached to the checkbox
 * @param classes - classes to be added to the input element
 * @param toggleFunction - function to be called in change listener
 * @returns "dark"/"light"
 */
export function generateCheckbox(
  label: string,
  classes: string[],
  toggleFunction: (a: boolean) => void,
) {
  const wrapper = document.createElement("label");
  const checkbox = document.createElement("input");
  const text = document.createElement("span");

  wrapper.classList.add("checkbox");
  checkbox.setAttribute("type", "checkbox");
  checkbox.setAttribute("checked", "true");
  classes.forEach((v) => checkbox.classList.add(v));
  text.innerText = label;
  text.classList.add("ml-2");
  wrapper.appendChild(checkbox);
  wrapper.appendChild(text);

  checkbox.addEventListener("change", () => {
    toggleFunction(checkbox.checked);
  });
  return wrapper;
}

export function setLoading(parent: HTMLElement) {
  const div = document.createElement("div");
  div.classList.add("is-flex-grow-1", "skeleton-block");
  parent.replaceChildren(div);
}
