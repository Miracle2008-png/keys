"""Real Python: decorators, f-strings, comprehensions, async."""
import asyncio
from dataclasses import dataclass

@dataclass
class Point:
    x: float = 0.0
    y: float = 0.0

    def distance(self) -> float:
        return (self.x ** 2 + self.y ** 2) ** 0.5

async def gather_points(n: int) -> list[Point]:
    # A comment with a "quoted string" inside it
    points = [Point(i, i * 2) for i in range(n) if i % 2 == 0]
    await asyncio.sleep(0.01)
    return points

if __name__ == "__main__":
    print(f"total = {len(asyncio.run(gather_points(10)))}")
