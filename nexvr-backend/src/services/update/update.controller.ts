import { Request, Response, NextFunction } from 'express';
import { z } from 'zod';
import { UpdateService } from './update.service.js';
import { ReleaseChannel } from '@prisma/client';

const updateService = new UpdateService();

const publishReleaseSchema = z.object({
  version: z.string().regex(/^\d+\.\d+\.\d+(-[a-zA-Z0-9.]+)?$/),
  channel: z.nativeEnum(ReleaseChannel).default(ReleaseChannel.STABLE),
  releaseNotes: z.string().optional(),
  isMandatory: z.boolean().default(false),
  installerKey: z.string().min(1),
  cliKey: z.string().optional(),
  dllKey: z.string().optional(),
  sha256Hash: z.string().length(64),
});

export class UpdateController {
  async check(req: Request, res: Response, next: NextFunction) {
    try {
      const version = (req.query.version as string) || '0.0.0';
      const channelParam = (req.query.channel as string)?.toUpperCase();
      const channel = Object.values(ReleaseChannel).includes(channelParam as ReleaseChannel)
        ? (channelParam as ReleaseChannel)
        : ReleaseChannel.STABLE;

      const result = await updateService.checkForUpdates(version, channel);
      res.status(200).json({ status: 'success', data: result });
    } catch (err) {
      next(err);
    }
  }

  async publish(req: Request, res: Response, next: NextFunction) {
    try {
      const data = publishReleaseSchema.parse(req.body);
      const release = await updateService.publishRelease(data);
      res.status(201).json({ status: 'success', data: release });
    } catch (err) {
      next(err);
    }
  }

  async getUploadPresigned(req: Request, res: Response, next: NextFunction) {
    try {
      const filename = req.query.filename as string;
      const contentType = (req.query.contentType as string) || 'application/octet-stream';
      if (!filename) {
        res.status(400).json({ status: 'error', message: 'Query parameter "filename" is required' });
        return;
      }
      const result = await updateService.getUploadUrl(filename, contentType);
      res.status(200).json({ status: 'success', data: result });
    } catch (err) {
      next(err);
    }
  }
}
