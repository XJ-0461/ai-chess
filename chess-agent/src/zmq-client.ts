import * as zmq from "zeromq";

export class ZmqClient {
  private socket: zmq.Pair;
  private endpoint: string;
  private isConnected: boolean = false;

  constructor(endpoint: string) {
    this.socket = new zmq.Pair();
    // Support either a port number (e.g. 5555) or a full ZMQ URI (e.g. tcp://127.0.0.1:5555)
    if (/^\d+$/.test(endpoint)) {
      this.endpoint = `tcp://127.0.0.1:${endpoint}`;
    } else {
      this.endpoint = endpoint;
    }
  }

  /**
   * Binds the PAIR socket to the server endpoint.
   */
  public async bind(): Promise<void> {
    console.log(`Binding ZMQ Pair socket to: ${this.endpoint}`);
    await this.socket.bind(this.endpoint);
    this.isConnected = true;
  }

  /**
   * Sends a JSON object to the server.
   */
  public async send(message: any): Promise<void> {
    const jsonStr = JSON.stringify(message);
    await this.socket.send(jsonStr);
  }

  /**
   * Async generator to listen for incoming messages.
   * Yields parsed JSON objects.
   */
  public async *receiveMessages(): AsyncGenerator<any, void, unknown> {
    for await (const [msg] of this.socket) {
      const msgStr = msg.toString();
      try {
        const parsed = JSON.parse(msgStr);
        yield parsed;
      } catch (err) {
        console.error("Failed to parse ZMQ message as JSON:", msgStr, err);
      }
    }
  }

  /**
   * Closes the socket connection.
   */
  public async close(): Promise<void> {
    if (this.isConnected) {
      try {
        this.socket.close();
      } catch (err) {
        console.error("Error closing ZMQ socket:", err);
      }
      this.isConnected = false;
    }
  }
}
