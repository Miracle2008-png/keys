// Real TypeScript: generics, unions, decorators, async/await.
interface Repository<T extends { id: number }> {
    find(id: number): Promise<T | undefined>;
}

type Status = "pending" | "active" | "closed";

class UserRepo implements Repository<{ id: number; name: string }> {
    private cache = new Map<number, { id: number; name: string }>();

    async find(id: number) {
        if (this.cache.has(id)) return this.cache.get(id);
        const res = await fetch(`/api/users/${id}`);
        return (await res.json()) as { id: number; name: string };
    }
}

const status: Status = "active";
console.log(`status is ${status}`, new UserRepo());
