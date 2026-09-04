import { Request, Response, NextFunction } from 'express';
import { z } from 'zod';
import { UserService } from './user.service.js';
import { Tier } from '@prisma/client';

const userService = new UserService();

const updateTierSchema = z.object({
  tier: z.nativeEnum(Tier),
});

const changePasswordSchema = z.object({
  currentPassword: z.string().min(1),
  newPassword: z.string().min(8).max(128),
});

export class UserController {
  async getProfile(req: Request, res: Response, next: NextFunction) {
    try {
      const profile = await userService.getUserProfile(req.user!.id);
      res.status(200).json({ status: 'success', data: profile });
    } catch (err) {
      next(err);
    }
  }

  async updateTier(req: Request, res: Response, next: NextFunction) {
    try {
      const data = updateTierSchema.parse(req.body);
      const updated = await userService.updateTier(req.user!.id, data.tier);
      res.status(200).json({ status: 'success', data: updated });
    } catch (err) {
      next(err);
    }
  }

  async changePassword(req: Request, res: Response, next: NextFunction) {
    try {
      const data = changePasswordSchema.parse(req.body);
      await userService.changePassword(req.user!.id, data.currentPassword, data.newPassword);
      res.status(200).json({ status: 'success', message: 'Password changed successfully' });
    } catch (err) {
      next(err);
    }
  }
}
