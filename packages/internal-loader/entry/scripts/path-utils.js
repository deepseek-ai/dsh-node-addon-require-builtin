'use strict';

function toPortablePath(value) {
  return value.replace(/\\/g, '/');
}

module.exports = {
  toPortablePath,
};
