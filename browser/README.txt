Usenet Archive Browser
======================

Qt5 based graphical archive browser.


Version 2016-11-17
------------------

The following improvements are available in this version:
- Each user name will now have semi-unique color.
- Number of threads is displayed next to total number of messages when opening
  new archive.
- Search scroll is now reset after performing new search.
- Notification window is displayed when loading archive.
- Ability to go to a specific date.
- Message viewing history has been added, with ability to go back to
  previously visited messages.
- Thread list is now filled on demand, which results in performance
  improvements and significant memory usage reduction.

Additionally, application state is now held between browsing sessions:
- Last browsed archive is remembered and reopened on application launch.
- Last viewed message is saved and restored when opening archive.
- Browser remembers which messages were already viewed and displays them
  accordingly.


Version 2016-08-30
------------------

Initial release.
