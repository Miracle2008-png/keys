// Real Go: goroutines, channels, interfaces, defer.
package main

import (
	"fmt"
	"sync"
)

type Shape interface {
	Area() float64
}

type Rect struct{ W, H float64 }

func (r Rect) Area() float64 { return r.W * r.H }

func main() {
	var wg sync.WaitGroup
	results := make(chan float64, 3)

	for _, s := range []Shape{Rect{2, 3}, Rect{4, 5}} {
		wg.Add(1)
		go func(shape Shape) {
			defer wg.Done()
			results <- shape.Area()
		}(s)
	}

	wg.Wait()
	close(results)
	fmt.Printf("done: %v\n", len(results))
}
