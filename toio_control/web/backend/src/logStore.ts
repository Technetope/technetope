import fs from "fs";
import path from "path";

export class LogStore<T extends { id: string }> {
  private entries: T[] = [];

  constructor(private readonly limit = 500) {}

  add(entry: T): T {
    this.entries.push(entry);
    if (this.entries.length > this.limit) {
      this.entries.splice(0, this.entries.length - this.limit);
    }
    return entry;
  }

  all(): T[] {
    return [...this.entries];
  }
}

export async function appendJsonl(pathname: string, payload: unknown): Promise<void> {
  await ensureDir(path.dirname(pathname));
  await fs.promises.appendFile(pathname, `${JSON.stringify(payload)}\n`, "utf-8");
}

async function ensureDir(dir: string): Promise<void> {
  await fs.promises.mkdir(dir, { recursive: true });
}
