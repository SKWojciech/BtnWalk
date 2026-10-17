# A simple 88x31 button walker in C++

Some people have shareable 88x31 buttons that link to their personal web page, blog or whatever.
That way, others can give them a shout out by putting that button on their own websites.

This program recursively fetches these buttons, creating a graphviz .dot file of how each website links to another.

## Isn't this the same as https://eightyeightthirty.one?

Yes... but I didn't know that existed before making it.
And this project is mine, and I had fun making it.

## Usage

    btnwalk <url> [-n max websites to visit] [-o output.dot]
