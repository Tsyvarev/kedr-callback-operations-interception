# KEDR callback operations interception
In the Linux kernel there are many essences implemented as objects which may be customized via callback operations set for them. E.g. file system in Linux is implemented mainly using such objects: inode's, dentry's, file's, etc.

Project aims to provide convenient mechanism to intercept such operations and execute user-provided code before or after them. This, e.g., may help in verification or debugging of Linux drivers which implements file systems or other subsystems of concrete type. Main features of interception mechanism:

It is dynamic, so it is not required to patch kernel code and recompile kernel.
Code which implements object with operations and code which implements interception handlers are fully independent: they may be written by different people and at different time.
Set of objects which operations are intended to intercept is fully controlled by the user: one may decide to intercept operations of only one object, or of all objects created by given driver.
This project is inspired as extending abilities of [KEDR framework](https://github.com/euspectre/kedr) (so 'kedr' prefix in the project name), but it may be efficiently used without KEDR.
