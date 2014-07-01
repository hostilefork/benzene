# Benzene Framework for Building Projectional Editors

(a.k.a. "Brian's Pathological Application Framework")

## AN IMPORTANT NOTICE

*This is being pushed to GitHub for development convenience, and issue tracking on the path forward.  It has been modified for Qt5 and to adapt to C++11 conventions via changes that were first tested in a simpler demo.  The sample apps will run, but they don't actually work...for starters [because changes to mouse handling have broken my plan](http://stackoverflow.com/questions/19498385/getting-a-mouse-drop-event-in-qt-5-if-mouse-doesnt-move-before-mouse-release)...*

*...but there are many more things that need to be changed.  The old code had become a mess after becoming a victim of my design experimentation once every year or two.  I don't hold the same beliefs about C/C++ programming style as I did in 2002, yet the codebase hasn't been fully audited for that.  Once those changes are patched in, it should be in tighter shape.  For now, don't bother trying to read or run it, unless you are me!*

## DESCRIPTION

Benzene is a C++ and Qt-based application framework, which imposes a great deal of internal architecture on how GUI editors are written.

Originally there were relatively few ideas in the framework.  Gradually it included more, and the test applications had to be continually updated to reflect each new design.  Like with many "grand experiments", it's the kind of thing that can seem fun for a good run until you hit a wall...then years pass and either you sort it out in your mind, or a new technology comes around.  And the cycle would begin again.

If you're a software formalist--then you are likely to empathize with many of the policies I've designed.  But right off the bat you will probably say it's weird to try doing so in C++ instead of some functional language.  And tell me to rewrite it in Haskell or something like that.

I'm sure there's merit in taking such an approach, and if I were to be starting such a thing here in the year 2010+ I would certainly take that approach.  But the project was started in C written directly to the Win32 API over a decade ago.  Then I'd pull it out and use it as my "hobby project" for whever I got the urge to try a new C++ library, concept, or language feature.  While it's been in the back of my mind for a long time, when I study new languages I tend to want to work on some new kind of problem...so I think this will never be anything but C++.

I also do have a suspicion that if I whipped it into shape well enough, it could be a shorter path for re-engineering existing C and C++ GUI editors to be better.  There's some useful program logic tied up in old MFC or GTK programs, and recasting it into Benzene's methods would be a shorter path...in the same way that C++ let people move along from C codebases a bit at a time.

But that's all very long off.  To learn more about the project, check out [benzene.hostilefork.com](http://benzene.hostilefork.com).

## LICENSE

License is under the GPLv3 for this initial source release.  Submodules have their own licenses, so read those to find out what they are.

I'm pretty sure that at this point the only person who can edit and comprehend the code for this is me.  So that's something I'm setting to turn around by taking my own advice on almost everything else: *"publish early, publish often"*.

Hopefully it will be beaten into good enough shape for at least a demo.
