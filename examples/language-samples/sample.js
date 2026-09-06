// Real JavaScript: closures, destructuring, async iteration, regex.
const RE_TOKEN = /^[A-Za-z_$][\w$]*$/u;

export class EventBus {
    #handlers = new Map();

    on(event, handler) {
        const list = this.#handlers.get(event) ?? [];
        this.#handlers.set(event, [...list, handler]);
        return () => this.off(event, handler);
    }

    async *stream(event, { signal } = {}) {
        while (!signal?.aborted) {
            yield await new Promise((resolve) => this.on(event, resolve));
        }
    }
}

const { name = "anon", ...rest } = { id: 1, tags: ["a"] };
console.log(`${name}:`, RE_TOKEN.test(name), rest);
