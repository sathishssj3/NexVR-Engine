import { db } from '../../common/db.js';
import { getPresignedDownloadUrl, getPresignedUploadUrl } from '../../common/storage.js';
import { getCached, setCached, invalidateCache } from '../../common/redis.js';
import { NotFoundError } from '../../common/errors.js';
import { ReleaseChannel } from '@prisma/client';

export class UpdateService {
  async checkForUpdates(currentVersion: string, channel: ReleaseChannel = ReleaseChannel.STABLE) {
    const cacheKey = `update:latest:${channel}`;
    let latest = await getCached<any>(cacheKey);

    if (!latest) {
      latest = await db.release.findFirst({
        where: { channel },
        orderBy: { createdAt: 'desc' },
      });
      if (latest) {
        await setCached(cacheKey, latest, 180);
      }
    }

    if (!latest) {
      return { updateAvailable: false, currentVersion };
    }

    const hasNewerVersion = this.compareSemver(latest.version, currentVersion) > 0;
    if (!hasNewerVersion) {
      return { updateAvailable: false, currentVersion };
    }

    const installerDownloadUrl = await getPresignedDownloadUrl(latest.windowsInstallerUrl);
    const cliDownloadUrl = latest.cliUrl ? await getPresignedDownloadUrl(latest.cliUrl) : null;
    const dllDownloadUrl = latest.dllUrl ? await getPresignedDownloadUrl(latest.dllUrl) : null;

    return {
      updateAvailable: true,
      currentVersion,
      latestVersion: latest.version,
      channel: latest.channel,
      releaseNotes: latest.releaseNotes,
      isMandatory: latest.isMandatory,
      sha256Hash: latest.sha256Hash,
      downloads: {
        installer: installerDownloadUrl,
        cli: cliDownloadUrl,
        dll: dllDownloadUrl,
      },
    };
  }

  async publishRelease(data: {
    version: string;
    channel: ReleaseChannel;
    releaseNotes?: string;
    isMandatory?: boolean;
    installerKey: string;
    cliKey?: string;
    dllKey?: string;
    sha256Hash: string;
  }) {
    const release = await db.release.upsert({
      where: { version: data.version },
      update: {
        channel: data.channel,
        releaseNotes: data.releaseNotes,
        isMandatory: data.isMandatory || false,
        windowsInstallerUrl: data.installerKey,
        cliUrl: data.cliKey,
        dllUrl: data.dllKey,
        sha256Hash: data.sha256Hash,
      },
      create: {
        version: data.version,
        channel: data.channel,
        releaseNotes: data.releaseNotes,
        isMandatory: data.isMandatory || false,
        windowsInstallerUrl: data.installerKey,
        cliUrl: data.cliKey,
        dllUrl: data.dllKey,
        sha256Hash: data.sha256Hash,
      },
    });

    await invalidateCache(`update:latest:${data.channel}`);
    return release;
  }

  async getUploadUrl(filename: string, contentType: string) {
    const key = `releases/raw/${Date.now()}-${filename}`;
    const uploadUrl = await getPresignedUploadUrl(key, contentType);
    return { key, uploadUrl };
  }

  private compareSemver(v1: string, v2: string): number {
    const clean1 = v1.replace(/^v/, '').split('.').map(Number);
    const clean2 = v2.replace(/^v/, '').split('.').map(Number);

    for (let i = 0; i < 3; i++) {
      const num1 = clean1[i] || 0;
      const num2 = clean2[i] || 0;
      if (num1 > num2) return 1;
      if (num1 < num2) return -1;
    }
    return 0;
  }
}
