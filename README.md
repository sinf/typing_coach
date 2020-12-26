# typing_coach
A command line program to practise your typing speed. It generates training text, measures your typing performance and saves typing data to a SQLite database for further analysis.  

## Compiling
Build it
```
$ cd src
$ make
$ make install
```
Create a database
```
$ typing_c -c MyDataBaseName -w MyWordList.txt
```
Start training
```
$ typing_c -d MyDataBaseName
```

## Data files
```
$(HOME)/.local/share/typingc/
├── MyDataBaseName.db
├── MyDataBaseName2.db
└── settings.ini
```


