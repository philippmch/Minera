// The miner wizards (Process models, Conformance) replace a step's cards with
// the next step's when one is chosen. The card that had focus is gone, so
// focus falls to <body> and a keyboard or screen-reader user starts again at
// the top of the page. After a step change, focus moves to the new step's
// title (or the breadcrumb's current step) when it was lost that way; a
// control that is still on the page (a breadcrumb button) keeps it.
import { useEffect, useRef } from "react";

/** Where focus lands in a wizard step, in order: its title, else the breadcrumb's current step. */
const STEP_TARGETS = [".miner-step-title", ".miner-trail .trail-item.current", ".miner-trail .trail-item.active"];

/**
 * Moves focus into `root`'s current step when focus was lost (it is on the
 * body, or on an element no longer in the document). Returns whether it moved.
 */
export function focusLostStep(root: HTMLElement | null, doc: Document = document): boolean {
  if (!root) return false;
  const active = doc.activeElement;
  if (active && active !== doc.body && active.isConnected) return false;
  let target: HTMLElement | null = null;
  for (const selector of STEP_TARGETS) if ((target = root.querySelector<HTMLElement>(selector))) break;
  if (!target) return false;
  // A title is not focusable by itself; -1 keeps it out of the tab order.
  if (!target.hasAttribute("tabindex") && !/^(BUTTON|A|INPUT|SELECT|TEXTAREA)$/.test(target.tagName)) target.setAttribute("tabindex", "-1");
  target.focus();
  return true;
}

/** A ref for a wizard's root that restores lost focus whenever `step` changes (not on mount). */
export function useStepFocus<T extends HTMLElement>(step: unknown) {
  const root = useRef<T>(null);
  const mounted = useRef(false);
  useEffect(() => {
    if (!mounted.current) { mounted.current = true; return; }
    focusLostStep(root.current);
  }, [step]);
  return root;
}
