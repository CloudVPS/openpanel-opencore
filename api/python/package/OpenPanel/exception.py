# This file is part of the OpenPanel API
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU Lesser General Public License as published by the
# Free Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/


class CoreException(Exception):
    def __init__(self, errorid, error):
        self.errorid = errorid
        self.error = error

    def __str__(self):
        return "%s (%d)" % (self.error, self.errorid)
