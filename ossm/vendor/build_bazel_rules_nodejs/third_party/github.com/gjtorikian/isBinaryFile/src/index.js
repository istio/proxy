"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
var __generator = (this && this.__generator) || function (thisArg, body) {
    var _ = { label: 0, sent: function() { if (t[0] & 1) throw t[1]; return t[1]; }, trys: [], ops: [] }, f, y, t, g;
    return g = { next: verb(0), "throw": verb(1), "return": verb(2) }, typeof Symbol === "function" && (g[Symbol.iterator] = function() { return this; }), g;
    function verb(n) { return function (v) { return step([n, v]); }; }
    function step(op) {
        if (f) throw new TypeError("Generator is already executing.");
        while (_) try {
            if (f = 1, y && (t = op[0] & 2 ? y["return"] : op[0] ? y["throw"] || ((t = y["return"]) && t.call(y), 0) : y.next) && !(t = t.call(y, op[1])).done) return t;
            if (y = 0, t) op = [op[0] & 2, t.value];
            switch (op[0]) {
                case 0: case 1: t = op; break;
                case 4: _.label++; return { value: op[1], done: false };
                case 5: _.label++; y = op[1]; op = [0]; continue;
                case 7: op = _.ops.pop(); _.trys.pop(); continue;
                default:
                    if (!(t = _.trys, t = t.length > 0 && t[t.length - 1]) && (op[0] === 6 || op[0] === 2)) { _ = 0; continue; }
                    if (op[0] === 3 && (!t || (op[1] > t[0] && op[1] < t[3]))) { _.label = op[1]; break; }
                    if (op[0] === 6 && _.label < t[1]) { _.label = t[1]; t = op; break; }
                    if (t && _.label < t[2]) { _.label = t[2]; _.ops.push(op); break; }
                    if (t[2]) _.ops.pop();
                    _.trys.pop(); continue;
            }
            op = body.call(thisArg, _);
        } catch (e) { op = [6, e]; y = 0; } finally { f = t = 0; }
        if (op[0] & 5) throw op[1]; return { value: op[0] ? op[1] : void 0, done: true };
    }
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.isBinaryFileSync = exports.isBinaryFile = void 0;
var fs = require("fs");
var util_1 = require("util");
var statAsync = util_1.promisify(fs.stat);
var openAsync = util_1.promisify(fs.open);
var closeAsync = util_1.promisify(fs.close);
var MAX_BYTES = 512;
function isBinaryFile(file, size) {
    return __awaiter(this, void 0, void 0, function () {
        var stat, fileDescriptor_1, allocBuffer_1;
        return __generator(this, function (_a) {
            switch (_a.label) {
                case 0:
                    if (!isString(file)) return [3, 3];
                    return [4, statAsync(file)];
                case 1:
                    stat = _a.sent();
                    isStatFile(stat);
                    return [4, openAsync(file, 'r')];
                case 2:
                    fileDescriptor_1 = _a.sent();
                    allocBuffer_1 = Buffer.alloc(MAX_BYTES);
                    return [2, new Promise(function (fulfill, reject) {
                            fs.read(fileDescriptor_1, allocBuffer_1, 0, MAX_BYTES, 0, function (err, bytesRead, _) {
                                closeAsync(fileDescriptor_1);
                                if (err) {
                                    reject(err);
                                }
                                else {
                                    fulfill(isBinaryCheck(allocBuffer_1, bytesRead));
                                }
                            });
                        })];
                case 3:
                    if (size === undefined) {
                        size = file.length;
                    }
                    return [2, isBinaryCheck(file, size)];
            }
        });
    });
}
exports.isBinaryFile = isBinaryFile;
function isBinaryFileSync(file, size) {
    if (isString(file)) {
        var stat = fs.statSync(file);
        isStatFile(stat);
        var fileDescriptor = fs.openSync(file, 'r');
        var allocBuffer = Buffer.alloc(MAX_BYTES);
        var bytesRead = fs.readSync(fileDescriptor, allocBuffer, 0, MAX_BYTES, 0);
        fs.closeSync(fileDescriptor);
        return isBinaryCheck(allocBuffer, bytesRead);
    }
    else {
        if (size === undefined) {
            size = file.length;
        }
        return isBinaryCheck(file, size);
    }
}
exports.isBinaryFileSync = isBinaryFileSync;
function isBinaryCheck(fileBuffer, bytesRead) {
    if (bytesRead === 0) {
        return false;
    }
    var suspiciousBytes = 0;
    var totalBytes = Math.min(bytesRead, MAX_BYTES);
    if (bytesRead >= 3 && fileBuffer[0] === 0xef && fileBuffer[1] === 0xbb && fileBuffer[2] === 0xbf) {
        return false;
    }
    if (bytesRead >= 4 && fileBuffer[0] === 0x00 && fileBuffer[1] === 0x00 && fileBuffer[2] === 0xfe && fileBuffer[3] === 0xff) {
        return false;
    }
    if (bytesRead >= 4 && fileBuffer[0] === 0xff && fileBuffer[1] === 0xfe && fileBuffer[2] === 0x00 && fileBuffer[3] === 0x00) {
        return false;
    }
    if (bytesRead >= 4 && fileBuffer[0] === 0x84 && fileBuffer[1] === 0x31 && fileBuffer[2] === 0x95 && fileBuffer[3] === 0x33) {
        return false;
    }
    if (totalBytes >= 5 && fileBuffer.slice(0, 5).toString() === '%PDF-') {
        return true;
    }
    if (bytesRead >= 2 && fileBuffer[0] === 0xfe && fileBuffer[1] === 0xff) {
        return false;
    }
    if (bytesRead >= 2 && fileBuffer[0] === 0xff && fileBuffer[1] === 0xfe) {
        return false;
    }
    for (var i = 0; i < totalBytes; i++) {
        if (fileBuffer[i] === 0) {
            return true;
        }
        else if ((fileBuffer[i] < 7 || fileBuffer[i] > 14) && (fileBuffer[i] < 32 || fileBuffer[i] > 127)) {
            if (fileBuffer[i] > 193 && fileBuffer[i] < 224 && i + 1 < totalBytes) {
                i++;
                if (fileBuffer[i] > 127 && fileBuffer[i] < 192) {
                    continue;
                }
            }
            else if (fileBuffer[i] > 223 && fileBuffer[i] < 240 && i + 2 < totalBytes) {
                i++;
                if (fileBuffer[i] > 127 && fileBuffer[i] < 192 && fileBuffer[i + 1] > 127 && fileBuffer[i + 1] < 192) {
                    i++;
                    continue;
                }
            }
            suspiciousBytes++;
            if (i > 32 && (suspiciousBytes * 100) / totalBytes > 10) {
                return true;
            }
        }
    }
    if ((suspiciousBytes * 100) / totalBytes > 10) {
        return true;
    }
    return false;
}
function isString(x) {
    return typeof x === "string";
}
function isStatFile(stat) {
    if (!stat.isFile()) {
        throw new Error("Path provided was not a file!");
    }
}
