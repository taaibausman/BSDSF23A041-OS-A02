v1.3.0 – Horizontal Column Display (-x)

**Q:** Vertical vs horizontal printing – which is harder and why?
**A:** Vertical printing needs pre-calculating rows/columns, so it’s more complex. Horizontal just prints left-to-right and wraps when needed.

**Q:** How does the program choose which display mode to use?
**A:** It uses an enum `DisplayMode` set during `getopt()` parsing and calls the corresponding function (`do_ls`, `do_ls_long`, `do_ls_horizontal`).


v1.4.0 – Alphabetical Sort

**Q:** Why read all filenames into memory before sorting? Any drawbacks?
**A:** Sorting needs all names in memory. Drawback: huge directories can use a lot of RAM.

**Q:** How does the `qsort()` comparison function work? Why `const void *`?
**A:** `qsort()` calls a function like `int cmp(const void *a, const void *b)` that casts to `char**` and compares strings with `strcmp()`. `const void *` makes it generic for any data type.


v1.5.0 – Colorized Output

**Q:** How do ANSI escape codes work? Example for green text?
**A:** They control terminal colors. Example: `printf("\033[0;32mHello\033[0m");` prints green.

**Q:** How to check if a file is executable?
**A:** Check `st_mode` bits: `S_IXUSR`, `S_IXGRP`, `S_IXOTH`. If any is set, the file is executable.


v1.6.0 – Recursive Listing (-R)

**Q:** What is a base case in recursion? Base case for recursive `ls`?
**A:** Base case stops recursion. Here, it’s when a directory has no subdirectories (or only `.` and `..`).

**Q:** Why build a full path before recursion?
**A:** So the function knows the correct location of the subdirectory. Calling `do_ls("subdir")` alone would look in the wrong place.
