function parseFieldKey(key) {
  const raw = String(key || "").trim();
  if (!raw) {
    return null;
  }
  const parts = raw.split(",").map((part) => part.trim());
  if (parts.length === 1 && parts[0]) {
    return {
      raw,
      shortName: parts[0],
      hasLevels: false,
      levtype: null,
      level: null,
      tupleKey: "__no_level__",
    };
  }
  if (parts.length !== 3 || !parts[0] || !parts[1] || !parts[2]) {
    return null;
  }
  return {
    raw,
    shortName: parts[0],
    hasLevels: true,
    levtype: parts[1],
    level: parts[2],
    tupleKey: `${parts[1]},${parts[2]}`,
  };
}

function buildFieldKey(shortName, meta) {
  if (!meta?.hasLevels) {
    return shortName;
  }
  return `${shortName},${meta.levtype},${meta.level}`;
}

function safeNumber(value) {
  const n = Number(value);
  return Number.isFinite(n) ? n : null;
}

function sortedUnique(arr) {
  return [...new Set(arr)].sort((a, b) => {
    const pa = parseFieldKey(a);
    const pb = parseFieldKey(b);
    if (!pa || !pb) {
      return String(a).localeCompare(String(b));
    }
    const shortCmp = pa.shortName.localeCompare(pb.shortName);
    if (shortCmp !== 0) {
      return shortCmp;
    }
    if (pa.hasLevels !== pb.hasLevels) {
      return pa.hasLevels ? -1 : 1;
    }
    if (!pa.hasLevels) {
      return 0;
    }
    const levCmp = pa.levtype.localeCompare(pb.levtype);
    if (levCmp !== 0) {
      return levCmp;
    }
    const na = Number(pa.level);
    const nb = Number(pb.level);
    if (Number.isFinite(na) && Number.isFinite(nb)) {
      return na - nb;
    }
    return pa.level.localeCompare(pb.level);
  });
}

export function getComputedWindSpeedFieldNames(fieldKeys) {
  const keys = Array.isArray(fieldKeys) ? fieldKeys : [];
  const groups = new Map();

  for (const key of keys) {
    const parsed = parseFieldKey(key);
    if (!parsed) {
      continue;
    }
    if (!groups.has(parsed.tupleKey)) {
      groups.set(parsed.tupleKey, {
        hasLevels: parsed.hasLevels,
        levtype: parsed.levtype,
        level: parsed.level,
        shortNames: new Set(),
      });
    }
    groups.get(parsed.tupleKey).shortNames.add(parsed.shortName);
  }

  const out = [];
  for (const group of groups.values()) {
    if (group.shortNames.has("10u") && group.shortNames.has("10v")) {
      out.push(buildFieldKey("10ws", group));
    }
    if (group.shortNames.has("100u") && group.shortNames.has("100v")) {
      out.push(buildFieldKey("100ws", group));
    }
    if (group.hasLevels && group.shortNames.has("u") && group.shortNames.has("v")) {
      out.push(buildFieldKey("ws", group));
    }
  }

  const existing = new Set(keys.map((key) => String(key)));
  return sortedUnique(out.filter((key) => !existing.has(String(key))));
}

function sourceShortNamesForComputed(shortName) {
  if (shortName === "10ws") {
    return ["10u", "10v"];
  }
  if (shortName === "100ws") {
    return ["100u", "100v"];
  }
  if (shortName === "ws") {
    return ["u", "v"];
  }
  return null;
}

export function ensureComputedWindSpeedFields(snapshot) {
  if (!snapshot || typeof snapshot !== "object") {
    return snapshot;
  }
  if (!snapshot.fields || typeof snapshot.fields !== "object") {
    return snapshot;
  }

  const fields = snapshot.fields;
  const allKeys = Object.keys(fields);
  const computedKeys = getComputedWindSpeedFieldNames(allKeys);

  for (const computedKey of computedKeys) {
    const parsed = parseFieldKey(computedKey);
    if (!parsed) {
      continue;
    }
    const sources = sourceShortNamesForComputed(parsed.shortName);
    if (!sources) {
      continue;
    }

    const uKey = buildFieldKey(sources[0], parsed);
    const vKey = buildFieldKey(sources[1], parsed);
    const uField = fields[uKey];
    const vField = fields[vKey];
    if (!uField || !vField) {
      continue;
    }

    const uValues = Array.isArray(uField.values) ? uField.values : null;
    const vValues = Array.isArray(vField.values) ? vField.values : null;
    if (!uValues || !vValues || uValues.length === 0 || uValues.length !== vValues.length) {
      continue;
    }

    const values = new Array(uValues.length);
    let valid = true;
    for (let i = 0; i < uValues.length; i += 1) {
      const u = safeNumber(uValues[i]);
      const v = safeNumber(vValues[i]);
      if (u === null || v === null) {
        valid = false;
        break;
      }
      values[i] = Math.hypot(u, v);
    }
    if (!valid) {
      continue;
    }

    fields[computedKey] = {
      ...uField,
      values,
    };
  }

  if (!Array.isArray(snapshot.field_keys)) {
    snapshot.field_keys = Object.keys(fields);
  } else {
    snapshot.field_keys = sortedUnique([...snapshot.field_keys, ...Object.keys(fields)]);
  }

  return snapshot;
}
