const mockDb = {
  release: {
    findFirst: jest.fn(),
    create: jest.fn(),
    upsert: jest.fn(),
  },
};

const mockStorage = {
  getPresignedDownloadUrl: jest.fn((key: string) => Promise.resolve(`https://storage.example.test/${key}?signed=true`)),
  getPresignedUploadUrl: jest.fn((key: string) => Promise.resolve(`https://storage.example.test/upload/${key}?signed=true`)),
};

const mockRedis = {
  getCached: jest.fn(),
  setCached: jest.fn(),
  invalidateCache: jest.fn(),
};

jest.mock('../src/common/db.js', () => ({
  db: mockDb,
}));

jest.mock('../src/common/storage.js', () => mockStorage);
jest.mock('../src/common/redis.js', () => mockRedis);
jest.mock('../src/common/logger.js', () => ({
  logger: {
    info: jest.fn(),
    warn: jest.fn(),
    error: jest.fn(),
  },
}));

import { UpdateService } from '../src/services/update/update.service.js';
import { ReleaseChannel } from '@prisma/client';

describe('UpdateService', () => {
  let updateService: UpdateService;

  beforeEach(() => {
    jest.clearAllMocks();
    updateService = new UpdateService();
  });

  describe('compareSemver', () => {
    it('correctly compares version components and handles pre-releases', () => {
      expect(updateService.compareSemver('0.1.97', '0.1.96')).toBeGreaterThan(0);
      expect(updateService.compareSemver('0.1.97', '0.1.97')).toBe(0);
      expect(updateService.compareSemver('0.1.90', '0.1.97')).toBeLessThan(0);
      expect(updateService.compareSemver('1.0.0', '0.9.9')).toBeGreaterThan(0);
      expect(updateService.compareSemver('0.2.0', '0.1.99')).toBeGreaterThan(0);
    });
  });

  describe('checkForUpdates', () => {
    it('returns updateAvailable: false when client has latest version', async () => {
      mockRedis.getCached.mockResolvedValue(null);
      mockDb.release.findFirst.mockResolvedValue({
        version: '0.1.97',
        channel: ReleaseChannel.STABLE,
        windowsInstallerUrl: 'releases/NexVR-Engine-Setup-0.1.97.exe',
      });

      const res = await updateService.checkForUpdates('0.1.97', ReleaseChannel.STABLE);

      expect(res.updateAvailable).toBe(false);
      expect(res.currentVersion).toBe('0.1.97');
    });

    it('returns updateAvailable: true with presigned URLs and sha256 when newer version is found', async () => {
      mockRedis.getCached.mockResolvedValue(null);
      mockDb.release.findFirst.mockResolvedValue({
        version: '0.1.97',
        channel: ReleaseChannel.STABLE,
        releaseNotes: 'Hotfix for 10-bit HDR format normalization',
        isMandatory: true,
        sha256Hash: 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
        windowsInstallerUrl: 'releases/NexVR-Engine-Setup-0.1.97.exe',
        cliUrl: 'releases/vr-inject-cli.exe',
        dllUrl: 'releases/vrinject.dll',
      });

      const res = await updateService.checkForUpdates('0.1.90', ReleaseChannel.STABLE);

      expect(res.updateAvailable).toBe(true);
      expect(res.latestVersion).toBe('0.1.97');
      expect(res.isMandatory).toBe(true);
      expect(res.sha256Hash).toBe('e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');
      expect(res.downloads?.installer).toContain('NexVR-Engine-Setup-0.1.97.exe');
      expect(res.downloads?.cli).toContain('vr-inject-cli.exe');
      expect(res.downloads?.dll).toContain('vrinject.dll');
      expect(mockRedis.setCached).toHaveBeenCalled();
    });

    it('uses cached release metadata from Redis when available', async () => {
      mockRedis.getCached.mockResolvedValue({
        version: '0.1.97',
        channel: ReleaseChannel.STABLE,
        windowsInstallerUrl: 'releases/cached.exe',
      });

      const res = await updateService.checkForUpdates('0.1.90', ReleaseChannel.STABLE);

      expect(res.updateAvailable).toBe(true);
      expect(mockDb.release.findFirst).not.toHaveBeenCalled();
    });
  });

  describe('publishRelease', () => {
    it('creates release entry in database and invalidates channel cache', async () => {
      mockDb.release.upsert.mockResolvedValue({
        id: 'rel_001',
        version: '0.1.97',
        channel: ReleaseChannel.STABLE,
      });

      const res = await updateService.publishRelease({
        version: '0.1.97',
        channel: ReleaseChannel.STABLE,
        releaseNotes: 'Official 0.1.97 hotfix',
        installerKey: 'releases/installer.exe',
        sha256Hash: 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
      });

      expect(mockDb.release.upsert).toHaveBeenCalled();
      expect(mockRedis.invalidateCache).toHaveBeenCalledWith('update:latest:STABLE');
      expect(res.id).toBe('rel_001');
    });
  });
});
