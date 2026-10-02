import { db } from '../../common/db.js';
import { NotFoundError, ForbiddenError } from '../../common/errors.js';
import { Prisma } from '@prisma/client';

export class ProfileService {
  async getProfilesForGame(gameId: string) {
    return await db.vRProfile.findMany({
      where: { gameId },
      include: {
        author: {
          select: { id: true, username: true },
        },
      },
      orderBy: [{ isOfficial: 'desc' }, { downloads: 'desc' }],
    });
  }

  async getProfileById(id: string) {
    const profile = await db.vRProfile.findUnique({
      where: { id },
      include: {
        game: true,
        author: {
          select: { id: true, username: true },
        },
      },
    });

    if (!profile) {
      throw new NotFoundError(`VR Profile '${id}' not found`);
    }

    // Increment download counter
    await db.vRProfile.update({
      where: { id },
      data: { downloads: { increment: 1 } },
    });

    return profile;
  }

  async createProfile(
    authorId: string,
    data: {
      gameId: string;
      name: string;
      description?: string;
      configJson: Prisma.InputJsonValue;
      isOfficial?: boolean;
    }
  ) {
    return await db.vRProfile.create({
      data: {
        gameId: data.gameId,
        authorId,
        name: data.name,
        description: data.description,
        configJson: data.configJson,
        isOfficial: data.isOfficial || false,
      },
    });
  }

  async updateProfile(
    profileId: string,
    userId: string,
    userRole: string,
    data: {
      name?: string;
      description?: string;
      configJson?: Prisma.InputJsonValue;
    }
  ) {
    const profile = await db.vRProfile.findUnique({ where: { id: profileId } });
    if (!profile) {
      throw new NotFoundError('VR Profile not found');
    }

    if (profile.authorId !== userId && userRole !== 'ADMIN') {
      throw new ForbiddenError('Cannot modify profile created by another user');
    }

    return await db.vRProfile.update({
      where: { id: profileId },
      data,
    });
  }

  async deleteProfile(profileId: string, userId: string, userRole: string) {
    const profile = await db.vRProfile.findUnique({ where: { id: profileId } });
    if (!profile) {
      throw new NotFoundError('VR Profile not found');
    }

    if (profile.authorId !== userId && userRole !== 'ADMIN') {
      throw new ForbiddenError('Cannot delete profile created by another user');
    }

    await db.vRProfile.delete({ where: { id: profileId } });
  }
}
