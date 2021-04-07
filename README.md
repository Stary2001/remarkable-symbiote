# What is this?
An attempt at shape recognition for the reMarkable tablet. It's pretty rough around the edges, so it won't win any user experience awards any time soon.  
Currently, the shape recognizer from [Xournal++](https://github.com/xournalpp/xournalpp/) is used - it operates on stroke data directly instead of trying to use bitmap recognition. You can see a description of how it works [here](https://sourceforge.net/p/xournal/support-requests/6/#64c7).

Eventually I might write my own shape recognizer...

# Building
`rmkit` is included as a submodule - make sure to clone using `--recursive`, or `git submodule update --init` after cloning.

`make docker` will use the rmkit Docker container and output into `artifacts/symbiote` - this is recommended.  
Otherwise, just running `make` will use the locally installed toolchain, and output into `build/symbiote`.

# Using
Currently there is literally no UI - run it via SSH, draw something, then tap anywhere to stop and draw out the shapes.

# Acknowledgements
* [rmkit](https://rmkit.dev) - a light library that handles most of the things I want to deal with
* [iago](https://rmkit.dev/apps/iago)/[lamp](https://rmkit.dev/apps/lamp) from rmkit - reference on how to fake input events, inspiration