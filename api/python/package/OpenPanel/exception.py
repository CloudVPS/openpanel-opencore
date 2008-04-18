class CoreException(Exception):
    def __init__(self, errorid, error):
        self.errorid = errorid
        self.error = error

    def __str__(self):
        return "%s (%d)" % (self.error, self.errorid)
