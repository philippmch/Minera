// Browser storage that never throws. localStorage can be missing, or its
// getter and methods can throw (cookies or site data blocked, some private
// modes, a full quota), so every access goes through these helpers and a
// failure simply means "nothing remembered".

function storage(): Storage | null {
  try {
    return typeof localStorage === "undefined" ? null : localStorage;
  } catch {
    return null;
  }
}

export function readStored(key: string): string | null {
  try {
    return storage()?.getItem(key) ?? null;
  } catch {
    return null;
  }
}

/** Remembers a value; returns false when the browser refused to store it. */
export function writeStored(key: string, value: string): boolean {
  try {
    const target = storage();
    if (!target) return false;
    target.setItem(key, value);
    return true;
  } catch {
    return false;
  }
}

export function removeStored(key: string): boolean {
  try {
    const target = storage();
    if (!target) return false;
    target.removeItem(key);
    return true;
  } catch {
    return false;
  }
}
