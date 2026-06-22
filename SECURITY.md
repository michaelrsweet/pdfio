Security Policy
===============

This file describes how security issues are reported and handled, and what the
expectations are for security issues reported to this project.


Supported Versions
------------------

All production releases of this software are subject to this security policy.
A production release is tagged and given a semantic version number of the
form:

    MAJOR.MINOR.PATCH

where "MAJOR" is an integer starting at 1 and "MINOR" and "PATCH" are integers
starting at 0.  A feature release has a "PATCH" value of 0, for example:

    1.0.0
    1.1.0
    2.0.0

Beta releases and release candidates are *not* production releases and use
semantic version numbers of the form:

    MAJOR.MINORbNUMBER
    MAJOR.MINORrcNUMBER

where "MAJOR" and "MINOR" identify the new feature release version number and
"NUMBER" identifies a beta or release candidate number starting at 1, for
example:

    1.0b1
    1.0b2
    1.0rc1


What is a Security Bug?
-----------------------

Not every bug is a security bug.

Certain bugs that might be considered security bugs in a program, such as bugs
that lead to a Denial of Service, are *not* considered security bugs simply
because this project *does not provide a service*.  Some might argue that, "my
server uses this library and the bug in this library causes a denial of
service for my server", however it is the responsibility of the *server* to
protect against DoS attacks, not a subordinate library, because only the
server knows what is an appropriate use of memory, CPU, time, and other
resources.

Similarly, bugs caused by incorrect API usage such as passing `NULL` pointers
where such pointers are not allowed, passing the wrong kinds of pointers or
objects to an API, or using a private API are not security bugs because they
are not caused by an attacker but by the developer.

Finally, bugs that only exist in unreleased (non-production) or inactive code
are not security bugs because they do not affect ordinary users.

If the bug you've found falls into one of these three categories, please report
the bug as an the ordinary GitHub issue at
<https://github.com/michaelrsweet/pdfio/issues>.


Reporting a Security Bug
------------------------

Vulnerabilities should be reported to the project security advisory page at
<https://github.com/michaelrsweet/pdfio/security/advisories>.

Provide details, impact, reproducer, affected versions, workarounds, and a
patch for the vulnerability, if applicable.

You can expect a response within 5 business days.


Responsible Disclosure
----------------------

With *responsible disclosure*, a security issue (and its fix) is disclosed
only after a mutually-agreed period of time (the "embargo date").  The issue
and fix are shared amongst and reviewed by the key stakeholders and the
CERT/CC.  Fixes are released to the public on the agreed-upon date.

Responsible disclosure is used for security issues in production releases
whose CVSS score is 7 or more.  Fixes for lesser security issues are typically
pushed to the public GitHub repository after review and included in the next
scheduled production release.
