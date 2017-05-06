# d1-alarm receiver

This is installed beneath `/opt/d1-alarm`.  The receiver runs all the
time, and when a button-press is detected it will invoke `run-parts`
against the directory `/opt/d1-alarm/part.d`.
