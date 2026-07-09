import path from 'node:path';

import type { BindingInfo, EntryApi } from '@esplus/node-addon-native-custom-loader';

const { createEntryApi } =
  require('@esplus/node-addon-native-custom-loader') as {
    createEntryApi(packageDir: string): EntryApi;
  };

export type { BindingInfo };

const api = createEntryApi(path.resolve(__dirname, '..'));

export function requireBuiltin(moduleId: string): unknown {
  return api.requireBuiltin(moduleId);
}

export function isAllowedInternalId(moduleId: string): boolean {
  return api.isAllowedInternalId(moduleId);
}

export function getBindingInfo(): Readonly<BindingInfo> {
  return api.getBindingInfo();
}

export default {
  requireBuiltin,
  isAllowedInternalId,
  getBindingInfo,
};
