export class ElectronRelease {
  constructor(version, packageName, platforms) {
    this.version = version;
    this.packageName = packageName;
    this.platforms = Object.freeze([...platforms]);
  }

  supports(platform) {
    return this.platforms.includes(platform);
  }

  assertDependency(manifest) {
    const expected = `npm:electron@${this.version}`;
    const actual = manifest.devDependencies?.[this.packageName];
    if (actual !== expected) {
      throw new Error(
        `${this.packageName} must be pinned to ${expected}, got ${actual}`,
      );
    }
  }
}

export class ElectronReleaseCatalog {
  constructor(releases) {
    this.releases = Object.freeze([...releases]);
  }

  select(versionList) {
    if (!versionList) return this.releases;
    const requested = versionList.split(/\s+/).filter(Boolean);
    return requested.map((version) => {
      const release = this.releases.find((item) => item.version === version);
      if (!release) throw new Error(`unsupported Electron test version: ${version}`);
      return release;
    });
  }

  versionsForPlatform(platform) {
    return this.releases
      .filter((release) => release.supports(platform))
      .map((release) => release.version);
  }
}

export const electronReleaseCatalog = new ElectronReleaseCatalog([
  new ElectronRelease('43.0.0', 'electron-43', [
    'darwin-arm64',
    'darwin-x64',
    'linux-arm64-gnu',
    'linux-x64-gnu',
    'win32-arm64-msvc',
    'win32-ia32-msvc',
    'win32-x64-msvc',
  ]),
  new ElectronRelease('44.0.0', 'electron-44', [
    'darwin-arm64',
    'darwin-x64',
    'linux-arm64-gnu',
    'linux-x64-gnu',
    'win32-arm64-msvc',
    'win32-x64-msvc',
  ]),
  new ElectronRelease('45.0.0-alpha.6', 'electron-45', [
    'darwin-arm64',
    'darwin-x64',
    'linux-arm64-gnu',
    'linux-x64-gnu',
    'win32-arm64-msvc',
    'win32-x64-msvc',
  ]),
]);
