#!/bin/bash

set -x

rsync -aP html index.html manual.html style.css build.html \
      $USER@web.sourceforge.net:/home/project-web/tclap/htdocs/v1.2
