'use strict';

const fs = require('fs');
const path = require('path');
const errno = require('./errno');

function createError(message, rootErr) {
  const err = new Error(message + (rootErr.errno ? ` (${errno[rootErr.errno]})` : ''));
  err.cause = rootErr;
  return err;
}

function getStats(filePath) {
  return new Promise((resolve, reject) => {
    fs.stat(filePath, (err, stats) => {
      if (err) {
        reject(createError(`Can't read directory/file at "${filePath}"`, err));
        return;
      }

      resolve(stats);
    });
  });
}

function processFile(filePath, opts) {
  return new Promise((resolve, reject) => {
    fs.readFile(filePath, 'utf8', (err, data) => {
      if (err) {
        reject(createError(`Can't read file at "${filePath}"`, err));
        return;
      }

      resolve(data);
    });
  })
    .then((data) => {
      let writeFile = false;
      let obj;
      try {
        obj = JSON.parse(data);
      } catch (err) {
        throw createError(`Malformed package.json file at "${filePath}"`, err);
      }

      Object.keys(obj).forEach((key) => {
        const shouldBeDeleted = opts.fields ? (opts.fields.indexOf(key) !== -1) : (key[0] === '_');
        if (shouldBeDeleted) {
          delete obj[key];
          writeFile = true;
        }
      });

      if (writeFile || opts.force) {
        return new Promise((resolve, reject) => {
          fs.writeFile(filePath, JSON.stringify(obj, null, '  '), (err) => {
            if (err) {
              reject(createError(`Can't write processed file to "${filePath}"`, err));
              return;
            }

            resolve({ rewritten: true });
          });
        });
      }

      return { rewritten: false };
    })
    .then(r => ({ filePath, rewritten: r.rewritten, success: true }),
      err => ({ filePath, err, success: false }));
}

function processDir(dirPath, opts) {
  return new Promise((resolve, reject) => {
    fs.readdir(dirPath, (err, files) => {
      if (err) {
        reject(createError(`Can't read directory at "${dirPath}"`, err));
        return;
      }

      resolve(files);
    });
  })
    .then(files => Promise.all(files.map((fileName) => {
      const filePath = path.join(dirPath, fileName);

      return getStats(filePath)
        .then((stats) => {
          if (stats.isDirectory()) {
            return processDir(filePath, opts);
          } else if (fileName === 'package.json') {
            return processFile(filePath, opts);
          }
          return undefined;
        });
    })))
    .then(results => results.reduce((arr, value) => {
      if (!value) {
        return arr;
      }

      if (value.constructor === Array) {
        return arr.concat(value);
      }

      arr.push(value);
      return arr;
    }, [{ dirPath, success: true }]))
    .catch(err => [{ dirPath, err, success: false }]);
}

function removeNPMAbsolutePaths(filePath, opts) {
  opts = opts || {}; // eslint-disable-line no-param-reassign

  if (!filePath) {
    return Promise.reject(new Error('Missing path. The first argument should be the path to a directory or a package.json file.'));
  }

  return getStats(filePath)
    .then((stats) => {
      if (stats.isDirectory()) {
        return processDir(filePath, opts);
      } else if (path.basename(filePath) === 'package.json') {
        return processFile(filePath, opts)
          .then(result => [result]);
      }

      throw new Error('Invalid path provided. The path should be a directory or a package.json file.');
    });
}

module.exports = removeNPMAbsolutePaths;
