import { Request, Response, NextFunction } from 'express';
import { z } from 'zod';
import { ProfileService } from './profile.service.js';

const profileService = new ProfileService();

const createProfileSchema = z.object({
  gameId: z.string().uuid(),
  name: z.string().min(1).max(100),
  description: z.string().optional(),
  configJson: z.record(z.any()),
  isOfficial: z.boolean().optional(),
});

const updateProfileSchema = z.object({
  name: z.string().min(1).max(100).optional(),
  description: z.string().optional(),
  configJson: z.record(z.any()).optional(),
});

export class ProfileController {
  async getByGame(req: Request, res: Response, next: NextFunction) {
    try {
      const profiles = await profileService.getProfilesForGame(req.params.gameId);
      res.status(200).json({ status: 'success', data: profiles });
    } catch (err) {
      next(err);
    }
  }

  async getById(req: Request, res: Response, next: NextFunction) {
    try {
      const profile = await profileService.getProfileById(req.params.id);
      res.status(200).json({ status: 'success', data: profile });
    } catch (err) {
      next(err);
    }
  }

  async create(req: Request, res: Response, next: NextFunction) {
    try {
      const data = createProfileSchema.parse(req.body);
      const isOfficial = req.user!.role === 'ADMIN' ? data.isOfficial : false;
      const profile = await profileService.createProfile(req.user!.id, {
        ...data,
        isOfficial,
      });
      res.status(201).json({ status: 'success', data: profile });
    } catch (err) {
      next(err);
    }
  }

  async update(req: Request, res: Response, next: NextFunction) {
    try {
      const data = updateProfileSchema.parse(req.body);
      const updated = await profileService.updateProfile(
        req.params.id,
        req.user!.id,
        req.user!.role,
        data
      );
      res.status(200).json({ status: 'success', data: updated });
    } catch (err) {
      next(err);
    }
  }

  async delete(req: Request, res: Response, next: NextFunction) {
    try {
      await profileService.deleteProfile(req.params.id, req.user!.id, req.user!.role);
      res.status(200).json({ status: 'success', message: 'Profile deleted successfully' });
    } catch (err) {
      next(err);
    }
  }
}
