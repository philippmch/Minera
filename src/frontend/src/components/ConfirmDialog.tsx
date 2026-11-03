import { useEffect, useId, useRef, type KeyboardEvent } from "react";

/**
 * An in-app confirmation for a step that discards work. It is modal: focus
 * starts on the safe choice, Tab stays inside, Escape cancels, and focus goes
 * back to where it was when the dialog closes. The dialog box itself takes
 * focus (tabIndex -1) when its text is clicked, so focus never falls to the
 * page behind it and Tab, Shift+Tab and Escape keep working.
 */
export function ConfirmDialog({ title, message, confirmLabel, cancelLabel = "Cancel", onConfirm, onCancel }: {
  title: string;
  message: string;
  confirmLabel: string;
  cancelLabel?: string;
  onConfirm: () => void;
  onCancel: () => void;
}) {
  const dialog = useRef<HTMLDivElement | null>(null);
  const cancel = useRef<HTMLButtonElement | null>(null);
  const titleId = useId(), messageId = useId();
  useEffect(() => {
    const previous = document.activeElement instanceof HTMLElement ? document.activeElement : null;
    cancel.current?.focus();
    return () => { if (previous?.isConnected) previous.focus(); };
  }, []);
  const onKeyDown = (event: KeyboardEvent<HTMLDivElement>) => {
    if (event.key === "Escape") { event.preventDefault(); event.stopPropagation(); onCancel(); return; }
    if (event.key !== "Tab") return;
    const buttons = Array.from(dialog.current?.querySelectorAll<HTMLButtonElement>("button:not([disabled])") ?? []);
    if (!buttons.length) return;
    const first = buttons[0], last = buttons[buttons.length - 1];
    if (event.shiftKey && document.activeElement === first) { event.preventDefault(); last.focus(); }
    else if (!event.shiftKey && document.activeElement === last) { event.preventDefault(); first.focus(); }
    else if (!buttons.includes(document.activeElement as HTMLButtonElement)) { event.preventDefault(); first.focus(); }
  };
  return (
    <div className="palette-veil confirm-veil" onMouseDown={(event) => { if (event.target === event.currentTarget) onCancel(); }}>
      <div ref={dialog} className="confirm-dialog" role="alertdialog" aria-modal="true" aria-labelledby={titleId} aria-describedby={messageId} tabIndex={-1} onKeyDown={onKeyDown}>
        <h2 id={titleId}>{title}</h2>
        <p id={messageId}>{message}</p>
        <div className="confirm-actions">
          <button ref={cancel} className="btn-ghost" onClick={onCancel}>{cancelLabel}</button>
          <button className="btn-open confirm-danger" onClick={onConfirm}>{confirmLabel}</button>
        </div>
      </div>
    </div>
  );
}
