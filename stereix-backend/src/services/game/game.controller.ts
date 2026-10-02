import { Request, Response, NextFunction } from 'express';
import { z } from 'zod';
import { GameService } from './game.service.js';

const gameService = new GameService();

const upsertGameSchema = z.object({
  title: z.string().min(1),
  slug: z.string().min(1),
  exeNames: z.array(z.string().min(1)),
  graphicsApis: z.array(z.string().min(1)),
  antiCheat: z.string().optional(),
  supported: z.boolean().default(true),
});

export class GameController {
  async listGames(_req: Request, res: Response, next: NextFunction) {
    try {
      const games = await gameService.listGames();
      res.status(200).json({ status: 'success', data: games });
    } catch (err) {
      next(err);
    }
  }

  async getGameBySlug(req: Request, res: Response, next: NextFunction) {
    try {
      const game = await gameService.getGameBySlug(req.params.slug);
      res.status(200).json({ status: 'success', data: game });
    } catch (err) {
      next(err);
    }
  }

  async detectByExe(req: Request, res: Response, next: NextFunction) {
    try {
      const exeName = req.query.exe as string;
      if (!exeName) {
        res.status(400).json({ status: 'error', message: 'Query param "exe" is required' });
        return;
      }
      const match = await gameService.detectGameByExecutable(exeName);
      res.status(200).json({ status: 'success', data: match });
    } catch (err) {
      next(err);
    }
  }

  async upsertGame(req: Request, res: Response, next: NextFunction) {
    try {
      const data = upsertGameSchema.parse(req.body);
      const game = await gameService.upsertGame(data);
      res.status(200).json({ status: 'success', data: game });
    } catch (err) {
      next(err);
    }
  }
}
