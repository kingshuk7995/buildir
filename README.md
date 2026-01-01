# buildir
a minimal build system

---

I wanted to explore how the build systems work, and thought of implementing.

It uses GNU make's basic syntax(only the basic not variables or such).

I focused to make the graph structure immutable. So technically static variables can be easily supported via modifying the [parser](/src/parse.hpp). That would be a fun exploration.

I also extended a little exploring caching the graph. In that way if the file isn't changed then I can reuse the already parsed graph.

I won't like to have full mutable graph for supporting variables fully, though maybe in future can try to have static variables through parsing time.

