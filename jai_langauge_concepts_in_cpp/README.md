# What this is:
This is a recreation of certain concepts/structures from the [Jai Programming Language](https://github.com/Jai-Community/Jai-Community-Library/wiki) by Jonathan Blow.
This recreates the idea of a global "Context" which stores allocators, including some basic data structures that use it, including Arrays, Hash_Tables, and Threads.
This example also includes an explanation of how to implement a unique "garbage collection" mechanism in C++ using this context strategy, along with plenty of comments explaining this as it compares to most languages take on the topic of "memory safety."

# How to run:
You can run this experiment by typing `docker compose up -d`, then go into the docker image using `docker exec -it jai_language_concepts_in_cpp /bin/bash`, `cd` into `dev` and run `./build.sh`. This will produce your executable inside of `.build` folder. Then, run it with `.build/main`

# Supplemental materials:
- Casey Muratori's explanation of why most languages [get it wrong](https://www.youtube.com/watch?v=f4ioc8-lDc0&t=4406s)
- Ginger Bill's explanation of why most languages [get it wrong](https://www.gingerbill.org/article/2020/06/21/the-ownership-semantics-flaw/)
