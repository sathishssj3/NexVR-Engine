import bcrypt from 'bcryptjs';
import { db } from '../../common/db.js';
import { NotFoundError, BadRequestError } from '../../common/errors.js';
import { Tier } from '@prisma/client';

export class UserService {
  async getUserProfile(userId: string) {
    const user = await db.user.findUnique({
      where: { id: userId },
      select: {
        id: true,
        username: true,
        email: true,
        role: true,
        tier: true,
        createdAt: true,
        profiles: {
          select: {
            id: true,
            name: true,
            gameId: true,
            isOfficial: true,
            downloads: true,
          },
        },
      },
    });

    if (!user) {
      throw new NotFoundError('User not found');
    }

    return user;
  }

  async updateTier(userId: string, tier: Tier) {
    const user = await db.user.update({
      where: { id: userId },
      data: { tier },
      select: {
        id: true,
        username: true,
        email: true,
        role: true,
        tier: true,
      },
    });

    return user;
  }

  async changePassword(userId: string, currentPass: string, newPass: string) {
    const user = await db.user.findUnique({ where: { id: userId } });
    if (!user) {
      throw new NotFoundError('User not found');
    }

    const isValid = await bcrypt.compare(currentPass, user.passwordHash);
    if (!isValid) {
      throw new BadRequestError('Incorrect current password');
    }

    const newHash = await bcrypt.hash(newPass, 12);
    await db.user.update({
      where: { id: userId },
      data: { passwordHash: newHash },
    });
  }
}
