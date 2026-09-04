import { Router } from 'express';
import { GameController } from './game.controller.js';
import { authenticate, requireRole } from '../auth/auth.middleware.js';

const router = Router();
const controller = new GameController();

router.get('/', controller.listGames);
router.get('/detect', controller.detectByExe);
router.get('/:slug', controller.getGameBySlug);
router.post('/', authenticate, requireRole('DEVELOPER', 'ADMIN'), controller.upsertGame);

export const gameRouter = router;
