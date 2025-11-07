import { WebSocketServer, WebSocket } from "ws";

import { WebsocketPush } from "./types.js";

export class WebsocketHub {
  private readonly clients = new Set<WebSocket>();

  constructor(private readonly wss: WebSocketServer) {
    this.wss.on("connection", (socket) => this.handleConnection(socket));
  }

  broadcast(payload: WebsocketPush): void {
    const message = JSON.stringify(payload);
    for (const client of this.clients) {
      if (client.readyState === WebSocket.OPEN) {
        client.send(message);
      }
    }
  }

  private handleConnection(socket: WebSocket): void {
    this.clients.add(socket);
    socket.on("close", () => this.clients.delete(socket));
  }
}
