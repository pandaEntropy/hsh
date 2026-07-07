## hsh - The Hollow Shell

*hsh* is a compact UNIX shell with a focus on state rollback.

State rollback is achieved by adapting the Memento pattern.

## Installation

*hsh* is a compact shell aimed to demonstrate state rollback in UNIX shells, and is thus not completely production ready.

*hsh* is ready for daily use, however there are limitations. For more information on limitations, see the limitations section below.

* Note: *hsh* can only run on Linux starting from version 2.2

1. To clone the repo run:

    `git clone https://github.com/pandaEntropy/hsh.git`

2. After cloning it, it is best to test it by running it as an executable rather than setting it up as the main shell.

    * To try it out while in the project directory, run: `./hsh`

## Testing

* *hsh* supports one operator per command, except for pipes which can be chained indefinitely.

* Supported operators are:

    | Symbol  | Name |
    | ------------- |:-------------:|
    | > | Redirection |
    | && | AND |
    | \| | Pipe |
    | \|\| | OR |
    | & | Background Operator |

* State rollback is achieved by running the builtin command `undo`.

* `undo`, reverts the changes made by the latest executed and rollback-supported command.

* `undo` can restore file content, permissions, time stamps and shell state where applicable.

* `undo` will not do anything if there is nothing to restore.

* The list of built-ins can be viewed by running `help`.

* Note that command flags are not supported, meaning any command you run with flags is not undoable.

## Technical Overview

* *hsh* adapts the Memento design pattern to achieve state rollback. Alterations had to be made to better suit the low-level C environment as the pattern itself was designed for object oriented programming.

* There are designated command categories according to which arguments are parsed. The parsed command category is then matched to the according memento type. There are 5 memento types: Create, Move, Delete, Internal and Hollow. Arguments are passed down to the dedicated memento creating function, which is the originator.

* Since mementos have to allocate space for numerous fields with the same lifetime, arena allocation is heavily utilized to optimize memento creation.

* Mementos are stores in an array, which functions as the undo stack. The Hollow type was introduced here to store a pointer to the memento as well as the type of the memento. The type is required later to unpack the Hollow memento.

* Hollow mementos are staged right before a command is executed, and are only pushed on the undo stack after a successful execution.

* When `undo` is executed, the top memento is popped from the stack. It is then unpacked and the dedicated rollback function is executed based on the memento type inside.

* Restoring destructive commands is possible with a trash directory located in `~/.local/share/hsh/trash`, as it is required for preserving file content. Trash is cleaned on exit.

## Current Limitations and Future Work

Currently, commands are mapped to their command typed in a table. This narrows the applicability of rollback, as supported commands have to be manually entered into the table. This is the best way I could come up with to pre-process commands. This could be avoided by post-processing commands, which would require dynamically reacting to the changes made without knowing anything about the command. My latest work on this is still in progress and involves system call intercpeting to achieve command post-processing.

Command behavior can be altered drastically by flags. The widely varying changes that flags bring is the reason they are omitted from rollback. Supporting flags would either require very advanced parsing, or it would be naturally supported by post-processing commands.

This project is centered around implementing state rollback in UNIX shells, thus it lacks support for complex parsing and quality of life features.
