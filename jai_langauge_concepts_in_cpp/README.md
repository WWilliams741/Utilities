# What this is:
This is a recreation of certain concepts/structures from the Jai programming language by Jonathan Blow.
This recreates the idea of a global "concept" which stores allocators, including some basic data structures that uses it, including Arrays, Hash_Tables, and Threads.
This example also includes an explanation of how to implement a unique "garbage collection" mechanism in C++ using this context strategy, along with plenty of comments explaining this as it compares to most languages take on the topic of "memory safety."

# How to run:
You can run this experiment by typing `docker compose up -d`, then going into the docker image and running ./build.sh. This will produce your executable. Then, run it with `.build/main`
