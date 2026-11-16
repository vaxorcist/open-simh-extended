# open-simh-extended/tools/crlfedit.py  v2
# Helper for editing the CR LF source files of SIMH from Python scripts.
# F(path) loads a file and keeps its text with LF line ends in .s;
# rep(a, b, cnt=1) replaces a by b and stops the script unless a occurs
# exactly cnt times (so an edit never lands in the wrong place); save()
# writes the file back with CR LF if it had CR LF.  Strings in the edit
# scripts use plain "\n".  Usage in a script:
#     import sys; sys.path.insert(0, "<notes>/tools")
#     from crlfedit import F
# v2 (2026-11-16): moved from Claude's workspace into the notes branch.
class F:
    def __init__(self, path):
        self.path = path
        raw = open(path, newline='').read()
        self.crlf = '\r\n' in raw
        self.s = raw.replace('\r\n', '\n')
    def rep(self, a, b, cnt=1):
        n = self.s.count(a)
        if n != cnt:
            raise SystemExit("%s: expected %d match(es), found %d for:\n%s" % (self.path, cnt, n, a[:300]))
        self.s = self.s.replace(a, b)
    def save(self):
        out = self.s.replace('\n', '\r\n') if self.crlf else self.s
        open(self.path, 'w', newline='').write(out)
