import { formatDistanceToNowStrict, parseISO } from "date-fns";

export function formatIso(iso?: string): string {
  if (!iso) {
    return "-";
  }
  try {
    const date = parseISO(iso);
    return date.toLocaleString();
  } catch {
    return iso;
  }
}

export function timeAgo(iso?: string): string {
  if (!iso) {
    return "unknown";
  }
  try {
    return `${formatDistanceToNowStrict(parseISO(iso))} ago`;
  } catch {
    return iso;
  }
}
