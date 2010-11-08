# This file is part of OpenPanel - The Open Source Control Panel
# The Grace library is free software: you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation, either version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

from OpenPanel.ext import simplejson
import cgi

class JSONFilter(object):
    def __init__(self, app, mime_type='text/x-json'):
        self.app = app
        self.mime_type = mime_type

    def __call__(self, environ, start_response):
        # Read JSON POST input to jsonfilter.json if matching mime type
        response = {'status': '200 OK', 'headers': []}
        def json_start_response(status, headers):
            response['status'] = status
            response['headers'].extend(headers)
        environ['jsonfilter.mime_type'] = self.mime_type
        if environ.get('REQUEST_METHOD', '') == 'POST':
            if environ.get('CONTENT_TYPE', '') == self.mime_type:
                args = [_ for _ in [environ.get('CONTENT_LENGTH')] if _]
                data = environ['wsgi.input'].read(*map(int, args))
                environ['jsonfilter.json'] = simplejson.loads(data)
        res = simplejson.dumps(self.app(environ, json_start_response))
        jsonp = cgi.parse_qs(environ.get('QUERY_STRING', '')).get('jsonp')
        if jsonp:
            content_type = 'text/javascript'
            res = ''.join(jsonp + ['(', res, ')'])
        elif 'Opera' in environ.get('HTTP_USER_AGENT', ''):
            # Opera has bunk XMLHttpRequest support for most mime types
            content_type = 'text/plain'
        else:
            content_type = self.mime_type
        headers = [
            ('Content-type', content_type),
            ('Content-length', len(res)),
        ]
        headers.extend(response['headers'])
        start_response(response['status'], headers)
        return [res]

def factory(app, global_conf, **kw):
    return JSONFilter(app, **kw)
