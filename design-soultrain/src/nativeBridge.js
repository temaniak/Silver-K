export const IS_NATIVE = Boolean(window.__SILVER_NATIVE__);
export function nativeSend(message) { if (IS_NATIVE && window.silverSend) window.silverSend(message); }
if (IS_NATIVE) document.body.classList.add('native-mode');
